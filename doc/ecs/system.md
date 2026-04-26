# Task system reference

This page is the task-side companion to
[`engine-token.md`](engine-token.md). The engine-token doc explains
*what* an `IEngineToken` is and *why* the gated / non-gated split
exists; this page explains *how a task uses one*. Specifically, how a
task receives a token through `ITask::setApiToken`, how it reaches the
gated and non-gated accessors through `ITask::apiToken()`, and how it must
behave when the FSM transitions out of the bound state.

> **Two realities to keep separate.** The R-StateScope contract on the
> token itself (gated accessors, expiration callbacks, alive flag) is
> already pinned down by the contract suite (scenario_21 /
> scenario_22) and the engine-token smoke test. The wiring that mints
> tokens and hands them to tasks lands in two stages:
> - **Current wiring (post-#334).** The per-tick token bound by
>   `TaskFlow::runCurrentTask` carries the sentinel-default
>   `vigine::statemachine::StateId{}` (rather than
>   `IStateMachine::current()`), and the owning
>   `unique_ptr<IEngineToken>` is destroyed at the end of the per-task
>   scope, so the pointer reachable through `apiToken()` does NOT outlive
>   the single `run()` call.
> - **Intended (post-#343 ITaskFlow redesign).** The per-tick mint
>   will carry `IStateMachine::current()` and the token will outlive
>   `run()`, so a worker thread that captured the pointer observes a
>   real mid-flight expiration on the next FSM transition.
>
> A task that needs FSM-bound expiration semantics today must mint a
> separate token through `IContext::makeEngineToken(stateId)` and own
> the `unique_ptr` itself (typically as a task member). The
> "Long-running task" example near the bottom of this page shows that
> shape; the rest of the page describes the per-tick token reachable
> through `apiToken()`.

## `ITask::apiToken()` subsection

### Surface (post-#324)

`ITask` ([`include/vigine/api/taskflow/itask.h`](../../include/vigine/api/taskflow/itask.h))
exposes three pure-virtual methods that together form the R-StateScope
contract on the task side:

```cpp
namespace vigine {

class ITask
{
  public:
    // Bound by the task flow before every run() invocation.
    // Pass nullptr to clear the binding.
    virtual void setApiToken(engine::IEngineToken *api) = 0;

    // Returns the bound token, or nullptr when no token is bound.
    [[nodiscard]] virtual engine::IEngineToken *apiToken() = 0;

    // Canonical task entry point. Invoked once per scheduled tick.
    [[nodiscard]] virtual Result run() = 0;
};

} // namespace vigine
```

Concrete tasks derive from
[`AbstractTask`](../../include/vigine/api/taskflow/abstracttask.h),
which implements `setApiToken` and `api` as `final` and leaves `run` pure
virtual. The token pointer lives on `AbstractTask::_api` (private,
held by composition); `setApiToken` and `api` are the only public surface
that touches it.

### Binding a token via `setApiToken(IEngineToken*)`

Tasks **never** call `setApiToken` themselves; the task flow is the only
caller. Ownership of the per-tick token's `unique_ptr<IEngineToken>`
sits on `TaskFlow::runCurrentTask`'s stack frame: the flow asks
`IContext` to mint a token, parks the unique_ptr on its own stack,
publishes a raw pointer to the task through `setApiToken`, runs the task,
clears the binding, and lets the unique_ptr fall out of scope at end
of scope. The state machine only contributes the invalidation-listener
registration the concrete `EngineToken` performs in its constructor;
it does not own the token's `unique_ptr`. The full path goes through
three layers:

- The engine's main pump
  ([`AbstractEngine::run`](../../src/api/engine/abstractengine.cpp))
  reads `IStateMachine::current()` each tick and looks up
  [`IStateMachine::taskFlowFor(current())`](../../include/vigine/api/statemachine/istatemachine.h).
  When a state-bound TaskFlow is registered (see
  [`addStateTaskFlow`](../../include/vigine/api/statemachine/istatemachine.h)),
  the engine advances it by exactly one task per tick.
- The flow's
  [`TaskFlow::runCurrentTask`](../../src/impl/taskflow/taskflow.cpp)
  is the per-task setApiToken/run/setApiToken(nullptr) site.
- The token is minted via `IContext::makeEngineToken`.

The per-tick wiring inside `TaskFlow::runCurrentTask`:

1. The task flow asks the context for a token through
   `IContext::makeEngineToken`. The current call site passes the
   invalid-sentinel `StateId{}` rather than the live FSM state — the
   `IStateMachine::current()` lookup that will seed the bound state
   inside `runCurrentTask` is flagged as a follow-up on
   [`src/api/taskflow/abstracttaskflow.cpp`](../../src/api/taskflow/abstracttaskflow.cpp).
   The engine's per-tick `taskFlowFor(current())` lookup already
   produces the per-state TaskFlow scoping described above, so a state
   transition between ticks switches WHICH flow is pumped without
   relying on the in-token state-id field. The
   `vigine::context::Context` aggregator factory tolerates the
   sentinel and threads it into the concrete `EngineToken`.
2. It calls the runnable's `setApiToken(token.get())` to publish that
   pointer to the task.
3. It invokes the runnable's `run()` synchronously while the binding
   is live.
4. An RAII `ApiBindingGuard` clears the binding (`setApiToken(nullptr)`)
   when the scope exits — even if `run()` throws.
5. The `unique_ptr<IEngineToken>` releases the token after the guard
   has run.

The order matters. `setApiToken(nullptr)` must run **before** the `unique_ptr`
releases the token, otherwise a stray callback on the task could
observe a dangling `IEngineToken*` through `apiToken()`. The guard pattern
in `AbstractTaskFlow::runCurrentTask` is the canonical reference:

```cpp
// src/impl/taskflow/taskflow.cpp (post-#324)
struct ApiBindingGuard {
    AbstractTask *task;
    explicit ApiBindingGuard(AbstractTask *t) : task(t) {}
    ~ApiBindingGuard() {
        if (task)
            task->setApiToken(nullptr);
    }
    // copy / move deleted
};

Result currStatus;
{
    _currTask->setApiToken(token.get());
    [[maybe_unused]] ApiBindingGuard guard(_currTask);
    currStatus = _currTask->run();
}
token.reset();
```

A task author does not write this code. The author writes `run()`; the
task flow drives the rest.

### Gated vs non-gated accessors via `apiToken()->...`

The token surface itself is documented in detail in
[`engine-token.md`](engine-token.md#hybrid-gating-model). The summary
below lists what a task reaches through `apiToken()->...` and what each
accessor returns.

#### Gated accessors (return `Result<T>` / `Result<T&>`)

These resources sit in registries the engine may recycle. Every call
checks `isAlive()` first and short-circuits to
`engine::Result::Code::Expired` after the FSM has transitioned away.

| Accessor                            | Resource                          | Failure modes                                  |
|-------------------------------------|-----------------------------------|------------------------------------------------|
| `apiToken()->service(ServiceId id)`      | `vigine::service::IService&`      | `Expired`, `NotFound`                          |
| `apiToken()->system(SystemId id)`        | `vigine::ecs::ISystem&`           | `Expired`, `Unavailable` (#197 follow-up wires the lookup) |
| `apiToken()->entityManager()`            | `vigine::IEntityManager&`         | `Expired`, `Unavailable` (#197 follow-up)      |
| `apiToken()->components()`               | `vigine::IComponentManager&`      | `Expired`, `Unavailable` (#197 follow-up)      |
| `apiToken()->ecs()`                      | `vigine::ecs::IECS&`              | `Expired` only                                 |

Callers branch on `outcome.ok()` (or `outcome.code()`) and pull the
live reference through `outcome.value()`:

```cpp
auto outcome = apiToken()->service(myServiceId);
if (!outcome.ok())
    return Result(Result::Code::Error, "service unavailable");
vigine::service::IService &svc = outcome.value();
```

#### Non-gated infrastructure accessors (return `T&`)

These resources are engine-lifetime singletons. They outlive every
state and remain valid even after the bound state has transitioned
away. **Reaching into a non-gated accessor after expiration is the
supported path for graceful drain — provided you still hold a
non-null token pointer when you do it.**

| Accessor                       | Resource                                              |
|--------------------------------|-------------------------------------------------------|
| `apiToken()->threadManager()`       | `vigine::core::threading::IThreadManager&`            |
| `apiToken()->systemBus()`           | `vigine::messaging::IMessageBus&`                     |
| `apiToken()->signalEmitter()`       | `vigine::messaging::ISignalEmitter&`                  |
| `apiToken()->stateMachine()`        | `vigine::statemachine::IStateMachine&`                |

Inside the body of `run()` the engine guarantees `apiToken()` is non-null
on the modern context path, so you can call these accessors directly:

```cpp
auto &tm = apiToken()->threadManager(); // safe inside run()
```

Outside `run()` (for example from a deferred callback or an event
handler that fires between ticks) the task flow has already cleared
the binding via `setApiToken(nullptr)`, so `apiToken()` returns `nullptr` even
for ungated reads. Null-check before dereferencing:

```cpp
if (auto *token = apiToken()) {
    auto &tm = token->threadManager();
    // ... use tm
}
```

### What happens when `apiToken()` is called after the FSM transitions

Two distinct cases:

1. **`apiToken()` itself returns `nullptr`.** Outside `run()` (for example
   from an event handler that fires between ticks) the task flow has
   already cleared the binding via `setApiToken(nullptr)`. The task **must**
   null-check the return value before dereferencing.
2. **`apiToken()` returns a non-null pointer but the FSM has transitioned
   away during `run()`.** The bound state was invalidated mid-tick.
   Gated accessors now report `engine::Result::Code::Expired` without
   touching the engine; non-gated accessors keep returning live
   references. The task should observe the typed `Expired` and return
   an error `Result` from `run()` so the flow does not advance.

When the engine boots a `vigine::context::Context`, the token bound
to your task is non-null and live for the duration of `run()`, so
null checks on `apiToken()` and `isAlive()` checks are not required for
the first access inside `run()`. They become required as soon as the
task posts work that may run on a different thread or after a future
state hop.

### Best-practice example

A task that resolves a service through the gated accessor, posts
follow-up work through the non-gated thread manager, and bails out
gracefully on expiration:

```cpp
#include "vigine/api/engine/iengine_token.h"
#include "vigine/api/taskflow/abstracttask.h"
#include "vigine/result.h"

namespace myproject {

class DecodeFrameTask final : public vigine::AbstractTask
{
  public:
    DecodeFrameTask(vigine::service::ServiceId workerId)
        : _workerId(workerId) {}

    [[nodiscard]] vigine::Result run() override
    {
        // 1. The task flow already called setApiToken() before this run().
        //    apiToken() is non-null and bound to the current state.
        auto *token = apiToken();
        if (token == nullptr)
            return vigine::Result(vigine::Result::Code::Error,
                                  "no engine token bound");

        // 2. Resolve a service through the gated accessor.
        auto outcome = token->service(_workerId);
        if (!outcome.ok()) {
            using Code = decltype(outcome)::Code;
            // Expired -- FSM has moved on. Bail out gracefully so the
            //            task flow does not advance through the
            //            success transition.
            // NotFound -- the worker id is invalid or its registry
            //             slot has been recycled.
            return vigine::Result(vigine::Result::Code::Error,
                                  outcome.code() == Code::Expired
                                      ? "decoder expired"
                                      : "decoder not found");
        }
        vigine::service::IService &worker = outcome.value();

        // 3. Reach an infrastructure resource through the non-gated
        //    accessor. This reference stays live even if the FSM
        //    transitions during the schedule call below; that is the
        //    supported drain path.
        auto &tm = token->threadManager();
        (void)tm; // (follow-up runnable schedule elided)

        // 4. Use the live service reference. Real work elided.
        (void)worker;
        return vigine::Result(vigine::Result::Code::Success);
    }

  private:
    vigine::service::ServiceId _workerId;
};

} // namespace myproject
```

The lifecycle the engine drives around this task is exactly:

1. Each tick `AbstractEngine::run` reads `IStateMachine::current()`
   and looks up the bound TaskFlow via
   `IStateMachine::taskFlowFor(...)`. When the FSM rests in a state
   that has a flow registered (and there is a current task to run),
   the engine calls `TaskFlow::runCurrentTask` to advance it by one.
2. The task flow mints a token through
   `IContext::makeEngineToken(StateId{})`. The argument is the
   sentinel-default `StateId{}` rather than `IStateMachine::current()`
   in the current wiring; the lookup that will swap in `current()`
   inside `runCurrentTask` is the open follow-up flagged on
   [`src/impl/taskflow/taskflow.cpp`](../../src/impl/taskflow/taskflow.cpp)
   and lands with #343. The task flow then calls
   `setApiToken(token.get())`.
3. The task flow calls `run()`. Inside, `apiToken()` returns the bound
   token; gated accessors resolve normally.
4. `run()` returns. The `ApiBindingGuard` in
   `TaskFlow::runCurrentTask` calls `setApiToken(nullptr)`. The owning
   `unique_ptr<IEngineToken>` then runs out of scope and **destroys
   the per-tick token**.
5. The engine drains queued FSM transitions on the controller thread.
   A `requestTransition(closeState)` posted from inside `run()` (or
   from a worker thread the task scheduled) lands here and flips
   `current()` to the new state. The next tick reads the new state
   and pumps a different bound TaskFlow — `Flow_Work` is no longer
   driven once the FSM has walked into `Flow_Close`.

So the **TaskFlow** lifecycle is per-state: which flow runs depends
on the FSM's active state, and the engine swap is automatic on
transition. The **per-tick token** lifecycle inside `runCurrentTask`
is bound to a single `run()` call: the unique_ptr lives on the flow's
stack frame and is destroyed before `runCurrentTask` returns. The
task only ever observed an `IEngineToken*` raw pointer through
`apiToken()`; that pointer is dangling the moment `run()` exits. Code that
captures the per-tick `apiToken()` pointer and hands it to a worker thread
therefore sees the token destroyed at the end of `run()`, and any
worker-side dereference of the captured pointer after that point is
undefined behaviour.

Deferred work the task posted from inside `run()` must do one of
three things:

- **Finish before `run()` returns.** If the work is short enough to
  complete synchronously, the per-tick token is still alive
  throughout.
- **Capture data, not the per-tick token.** Snapshot whatever the
  deferred work needs (an entity id, a service handle that owns its
  own lifetime, a message payload, a `shared_ptr` to a buffer the
  task manages) into the closure and let the closure reach back
  through some non-token mechanism — for example, a long-lived
  `IThreadManager` reference held elsewhere, or a posted signal that
  will be delivered through a fresh subscriber on the next tick.
- **Re-acquire the per-tick token on a future tick.** The next time
  the state-bound flow runs this task, the flow mints a new token
  and calls `setApiToken` again. Code that wants to resume work across
  ticks reads `apiToken()` fresh on each entry into `run()` rather than
  holding the previous pointer.
- **Mint a separate FSM-bound token through `IContext`.** When the
  worker thread genuinely needs a token whose lifetime outlives
  `run()`, the task mints its own
  `IContext::makeEngineToken(stateId)` and parks the
  `unique_ptr<IEngineToken>` on a task member. That token observes
  the full per-state lifecycle described in
  [`engine-token.md`](engine-token.md#tokens-minted-directly-through-icontextmakeenginetoken-per-state-fsm-driven):
  the FSM listener fires every registered expiration callback when
  the controller thread transitions out of the supplied `stateId`,
  and gated accessors short-circuit to `Result::Code::Expired` from
  that point on.

`subscribeExpiration` on the bound token (see
[`engine-token.md` § Self-destruct contract](engine-token.md#self-destruct-contract))
is only meaningful for tokens whose `unique_ptr` outlives the `run()`
body — i.e. the FSM-bound token a task minted itself through
`IContext::makeEngineToken` and parked on a member field. Calling
`subscribeExpiration` on the per-tick token reachable through `apiToken()`
is a programming error: the per-tick `unique_ptr` is destroyed at the
end of the per-task scope, so the subscription would fire (or be torn
down) at that destruction point rather than on a real FSM transition.
The contract scenarios at
[`test/contract/scenario_22_token_expiration_callback.cpp`](../../test/contract/scenario_22_token_expiration_callback.cpp)
pin down the exactly-once / controller-thread / before-flip
guarantees the FSM-bound subscription path relies on; the token under
test there is constructed directly via the `EngineToken` constructor
on a real registered state, mirroring the
`IContext::makeEngineToken(stateId)` minting path.

### Long-running task: state-bound work that observes `Expired` mid-flight

The example below adds a task that yields long-running work to the
thread pool and treats `Result::Code::Expired` on a gated accessor as
the cue to return cooperatively. The same pattern shows up in both
the docs' canonical render-cleanup example
([`engine-token.md` § Code example B](engine-token.md#b-long-running-render-task-that-releases-gpu-resources-on-transition))
and the contract suite's expiration-after-transition scenario
([scenario 21](../../test/contract/scenario_21_stale_engine_token.cpp)).

The shape below splits the token surface into two halves so the worker
thread observes a real FSM-driven expiration even though the per-tick
token reachable through `apiToken()` does not outlive `run()` in the
current wiring:

- The **per-tick token** (`apiToken()` inside `run()`) is used for the
  ungated `threadManager()` reach and to schedule the worker. It is
  not captured by the worker.
- The **FSM-bound token** (`_fsmToken` on the task instance) is
  minted through `IContext::makeEngineToken(stateId)` once and parked
  on a task member. The worker captures a raw pointer to it and
  observes `Result::Code::Expired` once the FSM transitions out of
  `stateId`. The expiration subscription is also registered against
  this token, so the cancellation callback fires synchronously on the
  controller thread when the transition is drained.

```cpp
#include "vigine/api/context/icontext.h"
#include "vigine/api/engine/iengine_token.h"
#include "vigine/api/statemachine/stateid.h"
#include "vigine/api/taskflow/abstracttask.h"
#include "vigine/result.h"

#include <atomic>
#include <memory>

namespace myproject {

class StreamFramesTask final : public vigine::AbstractTask
{
  public:
    StreamFramesTask(vigine::statemachine::StateId boundState,
                     vigine::service::ServiceId    decoderId)
        : _boundState(boundState), _decoderId(decoderId) {}

    [[nodiscard]] vigine::Result run() override
    {
        // Per-tick token: only valid for the body of run(), the
        // owning unique_ptr inside TaskFlow::runCurrentTask destroys
        // it the moment we return.
        auto *perTickToken = apiToken();
        if (perTickToken == nullptr)
            return vigine::Result(vigine::Result::Code::Error,
                                  "no per-tick engine token bound");

        // Resolve the decoder once per tick. Re-resolve on every
        // entry because the registry slot could have been recycled
        // between ticks (the per-tick token's bound-state field is
        // the sentinel today, so the gate cannot tell us anything
        // beyond "live"; the registry-side check is what would fire
        // on a recycle).
        auto outcome = perTickToken->service(_decoderId);
        if (!outcome.ok()) {
            using Code = decltype(outcome)::Code;
            return vigine::Result(vigine::Result::Code::Error,
                                  outcome.code() == Code::Expired
                                      ? "decoder reached on expired token"
                                      : "decoder unavailable");
        }
        vigine::service::IService &decoder = outcome.value();
        (void)decoder;

        // First-tick wiring: mint an FSM-bound token through
        // IContext::makeEngineToken(_boundState) and park it on a
        // task member so the worker thread can observe Expired on
        // the FSM transition out of _boundState. The unique_ptr lives
        // on the task instance, NOT on this run() stack frame.
        if (_fsmToken == nullptr) {
            _fsmToken = makeFsmBoundToken(_boundState);
            if (_fsmToken == nullptr)
                return vigine::Result(vigine::Result::Code::Error,
                                      "FSM-bound token unavailable");

            _buffer = std::make_shared<FrameBuffer>();
            _cancel = std::make_shared<std::atomic<bool>>(false);

            // Subscribe to expiration on the FSM-bound token. The
            // returned RAII subscription handle MUST be stored on a
            // member that outlives run(); dropping it here would
            // detach the callback before the worker even started.
            // The handle's destructor blocks on any in-flight
            // callback dispatch, so member-lifetime ownership keeps
            // the listener alive for the life of the task.
            _expiration = _fsmToken->subscribeExpiration(
                [cancel = _cancel]() {
                    cancel->store(true, std::memory_order_release);
                });

            // Schedule the worker on the engine thread pool. The
            // closure captures the FSM-bound token by raw pointer
            // (the unique_ptr stays alive on _fsmToken) plus the
            // shared cancel flag and frame buffer. The worker
            // observes Result::Code::Expired on the gated read once
            // the FSM transitions out of _boundState.
            auto &tm = perTickToken->threadManager();
            // Pseudo-code: see ITask::run() docstring on cooperative
            // exit and engine-token.md § Code example B for the full
            // worker-side gated-read shape.
            // (void)tm.schedule(makeRunnable(
            //     [token = _fsmToken.get(),
            //      buffer = _buffer,
            //      cancel = _cancel]() {
            //         while (!cancel->load(std::memory_order_acquire)) {
            //             auto frame = token->ecs();
            //             if (!frame.ok()) break;  // Expired on transition
            //             buffer->push(frame.value());
            //         }
            // }), vigine::core::threading::ThreadAffinity::Pool);
            (void)tm;
        }

        return vigine::Result(vigine::Result::Code::Success);
    }

  private:
    // Stand-in for "task asks IContext for an FSM-bound token". The
    // real wiring varies by application — see engine-token.md §
    // Code example B for the same factory note.
    static std::unique_ptr<vigine::engine::IEngineToken>
        makeFsmBoundToken(vigine::statemachine::StateId);

    struct FrameBuffer {
        void push(vigine::ecs::IECS &) {}
    };

    vigine::statemachine::StateId                          _boundState;
    vigine::service::ServiceId                             _decoderId;
    std::unique_ptr<vigine::engine::IEngineToken>          _fsmToken;
    std::shared_ptr<FrameBuffer>                           _buffer;
    std::shared_ptr<std::atomic<bool>>                     _cancel;
    std::unique_ptr<vigine::messaging::ISubscriptionToken> _expiration;
};

} // namespace myproject
```

The state-bound semantics in three lines: while the FSM rests in
`_boundState`, the engine pumps `run()` once per tick (per-tick
token comes and goes); the worker thread keeps making progress
across ticks against the FSM-bound token parked on the task instance;
the moment the FSM transitions away, the controller thread fires the
expiration callback registered on the FSM-bound token, the cancel
flag flips, the worker observes `Result::Code::Expired` on its next
gated read, and the engine starts pumping the new state's TaskFlow
on the very next tick.

Once the #343 ITaskFlow redesign threads `IStateMachine::current()`
into the per-tick mint and lets the per-tick token outlive `run()`,
the per-tick / FSM-bound split above collapses back into a single
token reached through `apiToken()`. Until then, the split is the
contract-safe shape.

## Cross-references

- Pure interface for the task-side contract:
  [`include/vigine/api/taskflow/itask.h`](../../include/vigine/api/taskflow/itask.h)
  (#321 / #324).
- Stateful base with the bound token member:
  [`include/vigine/api/taskflow/abstracttask.h`](../../include/vigine/api/taskflow/abstracttask.h)
  (#321 / #324).
- Engine-token surface and gating policy:
  [`doc/ecs/engine-token.md`](engine-token.md) (#220 / #287 / #319).
- Engine-token interface itself:
  [`include/vigine/api/engine/iengine_token.h`](../../include/vigine/api/engine/iengine_token.h)
  (#220).
- Engine-side per-tick pump that walks `taskFlowFor(current())` each
  tick and binds a token before `runCurrentTask`:
  [`src/api/engine/abstractengine.cpp`](../../src/api/engine/abstractengine.cpp)
  (#334).
- Per-state TaskFlow registry on the FSM:
  [`IStateMachine::addStateTaskFlow` / `taskFlowFor`](../../include/vigine/api/statemachine/istatemachine.h)
  (#334).
- Task-flow side that calls `setApiToken` / `run` and runs the
  `ApiBindingGuard`:
  [`src/impl/taskflow/taskflow.cpp`](../../src/impl/taskflow/taskflow.cpp)
  (#324 / #334 follow-up for the in-flow `current()` lookup).
- Token factory on the context:
  [`IContext::makeEngineToken`](../../include/vigine/api/context/icontext.h)
  (#286).
- Contract scenarios for stale token + expiration callback semantics:
  [`test/contract/scenario_21_stale_engine_token.cpp`](../../test/contract/scenario_21_stale_engine_token.cpp),
  [`test/contract/scenario_22_token_expiration_callback.cpp`](../../test/contract/scenario_22_token_expiration_callback.cpp).
- Engine smoke test scenarios 7-9 covering per-state TaskFlow pumping:
  [`test/engine/smoke_test.cpp`](../../test/engine/smoke_test.cpp)
  (#334).

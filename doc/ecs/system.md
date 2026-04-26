# Task system reference

This page is the task-side companion to
[`engine-token.md`](engine-token.md). The engine-token doc explains
*what* an `IEngineToken` is and *why* the gated / non-gated split
exists; this page explains *how a task uses one*. Specifically, how a
task receives a token through `ITask::setApi`, how it reaches the
gated and non-gated accessors through `ITask::api()`, and how it must
behave when the FSM transitions out of the bound state.

## `ITask::api()` subsection

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
    virtual void setApi(engine::IEngineToken *api) = 0;

    // Returns the bound token, or nullptr when no token is bound.
    [[nodiscard]] virtual engine::IEngineToken *api() = 0;

    // Canonical task entry point. Invoked once per scheduled tick.
    [[nodiscard]] virtual Result run() = 0;
};

} // namespace vigine
```

Concrete tasks derive from
[`AbstractTask`](../../include/vigine/api/taskflow/abstracttask.h),
which implements `setApi` and `api` as `final` and leaves `run` pure
virtual. The token pointer lives on `AbstractTask::_api` (private,
held by composition); `setApi` and `api` are the only public surface
that touches it.

### Binding a token via `setApi(IEngineToken*)`

Tasks **never** construct or destroy a token. The engine owns the
token's lifetime through the state machine; the task flow is the only
caller of `ITask::setApi`. The full path goes through three layers:

- The engine's main pump
  ([`AbstractEngine::run`](../../src/api/engine/abstractengine.cpp))
  reads `IStateMachine::current()` each tick and looks up
  [`IStateMachine::taskFlowFor(current())`](../../include/vigine/api/statemachine/istatemachine.h).
  When a state-bound TaskFlow is registered (see
  [`addStateTaskFlow`](../../include/vigine/api/statemachine/istatemachine.h)),
  the engine advances it by exactly one task per tick.
- The flow's
  [`TaskFlow::runCurrentTask`](../../src/impl/taskflow/taskflow.cpp)
  is the per-task setApi/run/setApi(nullptr) site.
- The token is minted via `IContext::makeEngineToken`.

The per-tick wiring inside `TaskFlow::runCurrentTask`:

1. The task flow asks the context for a token through
   `IContext::makeEngineToken`. The current call site passes the
   invalid-sentinel `StateId{}` rather than the live FSM state — the
   `IStateMachine::current()` lookup that will seed the bound state
   inside `runCurrentTask` is flagged as a follow-up on
   [`src/impl/taskflow/taskflow.cpp`](../../src/impl/taskflow/taskflow.cpp).
   The engine's per-tick `taskFlowFor(current())` lookup already
   produces the per-state TaskFlow scoping described above, so a state
   transition between ticks switches WHICH flow is pumped without
   relying on the in-token state-id field. On the legacy
   `vigine::Context` aggregator, the factory ignores the argument and
   returns `nullptr` unconditionally; on the modern
   `vigine::context::Context` aggregator, the factory tolerates the
   sentinel and threads it into the concrete `EngineToken`.
2. It calls `_currTask->setApi(token.get())` to publish that pointer
   to the task. When the legacy path returned `nullptr`, the task
   simply observes a null `api()`.
3. It invokes `_currTask->run()` synchronously while the binding is
   live.
4. An RAII `ApiBindingGuard` clears the binding (`setApi(nullptr)`)
   when the scope exits — even if `run()` throws.
5. The `unique_ptr<IEngineToken>` releases the token after the guard
   has run.

The order matters. `setApi(nullptr)` must run **before** the `unique_ptr`
releases the token, otherwise a stray callback on the task could
observe a dangling `IEngineToken*` through `api()`. The guard pattern
in `TaskFlow::runCurrentTask` is the canonical reference:

```cpp
// src/impl/taskflow/taskflow.cpp (post-#324)
struct ApiBindingGuard {
    AbstractTask *task;
    explicit ApiBindingGuard(AbstractTask *t) : task(t) {}
    ~ApiBindingGuard() {
        if (task)
            task->setApi(nullptr);
    }
    // copy / move deleted
};

Result currStatus;
{
    _currTask->setApi(token.get());
    [[maybe_unused]] ApiBindingGuard guard(_currTask);
    currStatus = _currTask->run();
}
token.reset();
```

A task author does not write this code. The author writes `run()`; the
task flow drives the rest.

### Gated vs non-gated accessors via `api()->...`

The token surface itself is documented in detail in
[`engine-token.md`](engine-token.md#hybrid-gating-model). The summary
below lists what a task reaches through `api()->...` and what each
accessor returns.

#### Gated accessors (return `Result<T>` / `Result<T&>`)

These resources sit in registries the engine may recycle. Every call
checks `isAlive()` first and short-circuits to
`engine::Result::Code::Expired` after the FSM has transitioned away.

| Accessor                            | Resource                          | Failure modes                                  |
|-------------------------------------|-----------------------------------|------------------------------------------------|
| `api()->service(ServiceId id)`      | `vigine::service::IService&`      | `Expired`, `NotFound`                          |
| `api()->system(SystemId id)`        | `vigine::ecs::ISystem&`           | `Expired`, `Unavailable` (#197 follow-up wires the lookup) |
| `api()->entityManager()`            | `vigine::IEntityManager&`         | `Expired`, `Unavailable` (#197 follow-up)      |
| `api()->components()`               | `vigine::IComponentManager&`      | `Expired`, `Unavailable` (#197 follow-up)      |
| `api()->ecs()`                      | `vigine::ecs::IECS&`              | `Expired` only                                 |

Callers branch on `outcome.ok()` (or `outcome.code()`) and pull the
live reference through `outcome.value()`:

```cpp
auto outcome = api()->service(myServiceId);
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
| `api()->threadManager()`       | `vigine::core::threading::IThreadManager&`            |
| `api()->systemBus()`           | `vigine::messaging::IMessageBus&`                     |
| `api()->signalEmitter()`       | `vigine::messaging::ISignalEmitter&`                  |
| `api()->stateMachine()`        | `vigine::statemachine::IStateMachine&`                |

Inside the body of `run()` the engine guarantees `api()` is non-null
on the modern context path, so you can call these accessors directly:

```cpp
auto &tm = api()->threadManager(); // safe inside run()
```

Outside `run()` (for example from a deferred callback or an event
handler that fires between ticks) the task flow has already cleared
the binding via `setApi(nullptr)`, so `api()` returns `nullptr` even
for ungated reads. Null-check before dereferencing:

```cpp
if (auto *token = api()) {
    auto &tm = token->threadManager();
    // ... use tm
}
```

### What happens when `api()` is called after the FSM transitions

Two distinct cases:

1. **`api()` itself returns `nullptr`.** Outside `run()` (for example
   from an event handler that fires between ticks) the task flow has
   already cleared the binding via `setApi(nullptr)`. The task **must**
   null-check the return value before dereferencing.
2. **`api()` returns a non-null pointer but the FSM has transitioned
   away during `run()`.** The bound state was invalidated mid-tick.
   Gated accessors now report `engine::Result::Code::Expired` without
   touching the engine; non-gated accessors keep returning live
   references. The task should observe the typed `Expired` and return
   an error `Result` from `run()` so the flow does not advance.

When the engine boots a modern `vigine::context::Context`, the token
bound to your task is non-null and live for the duration of `run()`,
so null checks on `api()` and `isAlive()` checks are not required for
the first access inside `run()`. They become required as soon as the
task posts work that may run on a different thread or after a future
state hop. On the legacy `vigine::Context` path
(`vigine::Context::makeEngineToken` returns `nullptr`; the path is
deprecated and scheduled for removal in #282), the binding may be
null — code that runs through that path should null-check `api()`
defensively, even on the very first access inside `run()`.

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
        // 1. The task flow already called setApi() before this run().
        //    api() is non-null and bound to the current state.
        auto *token = api();
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
2. The task flow mints a token (today: with sentinel `StateId{}`
   threaded through; the lookup that swaps in `current()` inside
   `runCurrentTask` is the open follow-up flagged on
   [`src/impl/taskflow/taskflow.cpp`](../../src/impl/taskflow/taskflow.cpp))
   and calls `setApi(token.get())`.
3. The task flow calls `run()`. Inside, `api()` returns the bound
   token; gated accessors resolve normally.
4. `run()` returns. The `ApiBindingGuard` in
   `TaskFlow::runCurrentTask` calls `setApi(nullptr)`. The owning
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
is still bound to a single `run()` call. Code that captures the token
pointer and hands it to a worker thread therefore sees the token
either:

- destroyed at the end of `run()` (worker observes a dangling pointer
  unless it captured by `shared_ptr` to its own state); or
- still live across a tick boundary if the worker holds a strong
  reference and the bound state has not yet transitioned. Once the
  controller thread drains a transition out of the bound state, every
  still-live token (including the one captured by the worker) flips
  to expired and gated accessors short-circuit to `Expired`.

Deferred work the task posted from inside `run()` must do one of
three things:

- **Finish before `run()` returns.** If the work is short enough to
  complete synchronously, the per-tick token is still alive
  throughout.
- **Capture data, not the token.** Snapshot whatever the deferred
  work needs (an entity id, a service handle that owns its own
  lifetime, a message payload, a `shared_ptr` to a buffer the task
  manages) into the closure and let the closure reach back through
  some non-token mechanism — for example, a long-lived
  `IThreadManager` reference held elsewhere, or a posted signal that
  will be delivered through a fresh subscriber on the next tick.
- **Re-acquire the token on a future tick.** The next time the
  state-bound flow runs this task, the flow mints a new token and
  calls `setApi` again. Code that wants to resume work across ticks
  reads `api()` fresh on each entry into `run()` rather than holding
  the previous pointer.

`subscribeExpiration` on the bound token (see
[`engine-token.md` § Self-destruct contract](engine-token.md#self-destruct-contract))
is most useful for tokens that **outlive** the `run()` body — a
long-running task whose worker thread captures the token (or its
expiration subscription handle) by `shared_ptr` and watches for the
FSM to walk away from the bound state. Once the controller thread
drains the transition, the callback fires synchronously on that
thread, the alive flag flips, and gated accessors on the worker side
return `Expired`. The contract scenarios at
[`test/contract/scenario_22_token_expiration_callback.cpp`](../../test/contract/scenario_22_token_expiration_callback.cpp)
pin down the exactly-once / controller-thread / before-flip
guarantees this code relies on. Inside the legacy in-`run()`-only
shape (no worker capture), the per-tick token still expires as part
of the destruction at end of `run()`, so a same-tick subscription
will fire (or be torn down) immediately.

### Long-running task: state-bound work that observes Stale mid-flight

The example below adds a task that yields long-running work to the
thread pool and treats `Expired` on a gated accessor as the cue to
return cooperatively. The same pattern shows up in both the docs'
canonical render-cleanup example
([`engine-token.md` § Code example B](engine-token.md#code-example))
and the contract suite's stale-token scenario
([scenario 21](../../test/contract/scenario_21_stale_engine_token.cpp)).

```cpp
#include "vigine/api/engine/iengine_token.h"
#include "vigine/api/taskflow/abstracttask.h"
#include "vigine/result.h"

#include <atomic>
#include <memory>

namespace myproject {

class StreamFramesTask final : public vigine::AbstractTask
{
  public:
    StreamFramesTask(vigine::service::ServiceId decoderId)
        : _decoderId(decoderId) {}

    [[nodiscard]] vigine::Result run() override
    {
        auto *token = api();
        if (token == nullptr)
            return vigine::Result(vigine::Result::Code::Error,
                                  "no engine token bound");

        // Resolve the decoder once per tick. Even with the per-tick
        // token shape, every entry into run() sees a fresh,
        // state-bound token; we re-resolve because the registry slot
        // could have been recycled between ticks.
        auto outcome = token->service(_decoderId);
        if (!outcome.ok()) {
            using Code = decltype(outcome)::Code;
            // FSM-bound task: Expired means the engine is about to
            // pump a different state's flow. Bail out cooperatively
            // so the engine can swap us off the schedule.
            return vigine::Result(vigine::Result::Code::Error,
                                  outcome.code() == Code::Expired
                                      ? "stream task observed Expired"
                                      : "decoder unavailable");
        }
        vigine::service::IService &decoder = outcome.value();
        (void)decoder;

        // Yield a frame's worth of work to the thread pool. The
        // closure captures a shared_ptr to the task's frame buffer
        // and a non-owning pointer to the token; the cancel flag is
        // the cooperative-exit signal flipped by the expiration
        // callback below.
        auto buffer = std::make_shared<FrameBuffer>();
        auto cancel = std::make_shared<std::atomic<bool>>(false);

        auto sub = token->subscribeExpiration(
            [cancel]() { cancel->store(true, std::memory_order_release); });
        // Persist the subscription on the task so it outlives run(),
        // otherwise the RAII handle dropping here would detach the
        // callback before the worker even started.
        _expiration = std::move(sub);

        auto &tm = token->threadManager();
        // Pseudo-code: see ITask::run() docstring on co-operative
        // exit and engine-token.md § Code example B for the full
        // shape of the worker-side gated read.
        // (void)tm.schedule(makeRunnable([token, buffer, cancel]() {
        //     while (!cancel->load(std::memory_order_acquire)) {
        //         auto frame = token->ecs(); // Expired on transition
        //         if (!frame.ok()) break;
        //         buffer->push(frame.value());
        //     }
        // }), vigine::core::threading::ThreadAffinity::Pool);
        (void)tm;

        return vigine::Result(vigine::Result::Code::Success);
    }

  private:
    struct FrameBuffer {
        void push(vigine::ecs::IECS &) {}
    };

    vigine::service::ServiceId                             _decoderId;
    std::unique_ptr<vigine::messaging::ISubscriptionToken> _expiration;
};

} // namespace myproject
```

The state-bound semantics in three lines: while the FSM rests in the
state this task's TaskFlow is bound to, the engine pumps `run()` once
per tick; the worker thread keeps making progress across ticks; the
moment the FSM transitions away, the controller thread fires the
expiration callback, the cancel flag flips, the worker observes
`Expired` on its next gated read, and the engine starts pumping the
new state's TaskFlow on the very next tick.

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
- Task-flow side that calls `setApi` / `run` and runs the
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

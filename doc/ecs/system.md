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
caller of `ITask::setApi`. The wiring lives in
[`TaskFlow::runCurrentTask`](../../src/impl/taskflow/taskflow.cpp):

1. The task flow asks the context for a token bound to the current
   state via `IContext::makeEngineToken`.
2. It calls `_currTask->setApi(token.get())` to publish that pointer
   to the task.
3. It invokes `_currTask->run()` synchronously, inside the bound
   state's scope.
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
supported path for graceful drain.**

| Accessor                       | Resource                                              |
|--------------------------------|-------------------------------------------------------|
| `api()->threadManager()`       | `vigine::core::threading::IThreadManager&`            |
| `api()->systemBus()`           | `vigine::messaging::IMessageBus&`                     |
| `api()->signalEmitter()`       | `vigine::messaging::ISignalEmitter&`                  |
| `api()->stateMachine()`        | `vigine::statemachine::IStateMachine&`                |

```cpp
auto &tm = api()->threadManager(); // safe even if api() has expired
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

Inside the body of `run()` immediately after the task flow called
`setApi`, the engine guarantees the token is bound and live; null
checks on `api()` and `isAlive()` checks are not required for the
first access. They become required as soon as the task posts work
that may run on a different thread or after a future state hop.

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
        auto &tm = token->stateMachine();
        (void)tm; // (state machine inspection elided)

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

1. The task flow calls `setApi(token.get())` with a token bound to
   the current FSM state.
2. The task flow calls `run()`. Inside, `api()` returns the bound
   token; gated accessors resolve normally.
3. `run()` returns. The `ApiBindingGuard` in
   `TaskFlow::runCurrentTask` calls `setApi(nullptr)`. The owning
   `unique_ptr<IEngineToken>` releases the token.
4. After expiration, any deferred work the task posted to
   `threadManager()` or `systemBus()` keeps running through the
   non-gated singletons; any deferred work that calls back through
   `api()->service(...)` observes `engine::Result::Code::Expired`
   and bails out cleanly.

A task that needs an explicit clean-up hook on state-exit registers
it through `subscribeExpiration` on the bound token; see
[`engine-token.md` § Self-destruct contract](engine-token.md#self-destruct-contract)
for that contract.

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
- Task-flow side that calls `setApi` / `run` and runs the
  `ApiBindingGuard`:
  [`src/impl/taskflow/taskflow.cpp`](../../src/impl/taskflow/taskflow.cpp)
  (#324).
- Token factory on the context:
  [`IContext::makeEngineToken`](../../include/vigine/api/context/icontext.h)
  (#286).

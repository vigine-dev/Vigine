# Thread affinity

`ThreadAffinity` controls where a runnable executes when handed to
`IThreadManager`. A caller never picks a physical OS thread by name; it
picks an affinity value, and the manager decides which underlying thread
will pull the runnable off its queue. The set is deliberately closed —
new affinities require an architectural decision rather than a caller
opt-in — and every value here is the only legal scheduling target for
the engine kernel.

The enum lives in
[`include/vigine/core/threading/threadaffinity.h`](../../include/vigine/core/threading/threadaffinity.h)
and is consumed exclusively through
[`IThreadManager::schedule`](../../include/vigine/core/threading/ithreadmanager.h)
and its sibling `scheduleOnNamed`.

## Per-value semantics

| Value       | Routing                                                                                                                        | Notes                                                                              |
|-------------|--------------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------|
| `Any`       | Same as `Pool` at engine kernel; `TaskFlow` signal-routing re-interprets as sync-on-emitter                                    | Default value of `schedule(...)`.                                                  |
| `Pool`      | Shared pool queue; first available worker picks up                                                                             | Multiple workers; no per-caller order guarantee across workers                     |
| `Main`      | Main-thread queue, drained by `runMainThreadPump()`                                                                            | Use for callbacks needing main thread (windowing)                                  |
| `Dedicated` | Fresh OS thread per `schedule` call (capped by `maxDedicatedThreads`)                                                          | Per-call; not pooled                                                               |
| `Named`     | `schedule()` returns Error — must use `scheduleOnNamed(runnable, NamedThreadId)`                                               | Routes runnables to a registered named thread                                      |

### `Any`

The default. At the `IThreadManager::schedule` entry point, `Any` is
treated identically to `Pool`: the runnable lands on the shared pool
queue and the first free worker picks it up. Reach for `Any` whenever a
runnable has no thread-of-execution constraint and the caller does not
care which worker runs it. Avoid `Any` when ordering matters across
multiple submissions — pool workers race on the queue, so two `Any`
submissions from the same caller can complete out of submission order.

### `Pool`

Explicit request to land on the pool. Functionally identical to `Any`
at the kernel level today; the distinction exists so call sites that
*intend* pool scheduling read clearly versus call sites that simply do
not care. Use `Pool` for short, CPU-bound, embarrassingly parallel work
— per-entity updates, batched system passes, image decompression. Avoid
`Pool` for long-running tasks that would block a worker for many
milliseconds; that starves other pool consumers. The `parallelFor`
helper in [`parallel-for.md`](parallel-for.md) builds on `Pool`.

### `Main`

The runnable lands in an MPSC queue and waits there until some thread
calls `IThreadManager::runMainThreadPump()`, at which point every
queued runnable executes on the pumping thread in submission order.
"Main thread" is a scheduling contract, not a runtime identity check.
Use `Main` for callbacks that must run on the application's main thread
— typically windowing-system integration on platforms (macOS, some
Linux WM configurations) where window and input APIs reject calls from
any other thread. Avoid `Main` for compute work; pumping is single-
threaded and a heavy runnable stalls the next tick.

### `Dedicated`

A fresh OS thread is spun up per `schedule` call, runs the single
runnable, and exits. Thread count is capped by
`ThreadManagerConfig::maxDedicatedThreads` — once the cap is reached,
the manager rejects further `Dedicated` submissions with an error
`Result`. Use `Dedicated` for work that needs full thread isolation
without registering a long-lived named thread — typically a one-off
blocking call (a slow library that pins a thread). Avoid `Dedicated`
for hot paths; thread creation is expensive and the slot is not pooled.
For predictable per-caller FIFO, register a named thread instead.

### `Named`

The `schedule(runnable, ThreadAffinity::Named)` path returns an error
`Result` because the generic call cannot carry a `NamedThreadId`. Use
`scheduleOnNamed(runnable, id)` instead, where `id` is the value
returned from `IThreadManager::registerNamedThread("audio")` or similar.
A named thread is a long-lived OS thread that processes its inbox in
strict FIFO order — exactly what subsystems with a "single-threaded
API" contract want (audio mixers, dedicated I/O, GPU command recorders
on backends that pin a thread). Stale or unregistered ids return a
failing handle and the runnable is not executed.

## Dispatch matrix

```
IThreadManager::schedule(runnable, affinity)
  |
  +-- Any        -> Pool queue          -> ITaskHandle (ok)
  +-- Pool       -> Pool queue          -> ITaskHandle (ok)
  +-- Main       -> Main MPSC queue     -> ITaskHandle (ok, runs on next pump)
  +-- Dedicated  -> fresh OS thread     -> ITaskHandle (ok / error if cap hit)
  +-- Named      -> -                   -> ITaskHandle (error: wrong entry)

IThreadManager::scheduleOnNamed(runnable, NamedThreadId id)
  |
  +-- valid id   -> Named thread inbox  -> ITaskHandle (ok)
  +-- stale id   -> -                   -> ITaskHandle (error)
  +-- invalid id -> -                   -> ITaskHandle (error)
```

Every path returns an `ITaskHandle`; failures surface through the
handle's `wait()` `Result`, never as exceptions. After `shutdown()`,
every entry point returns a failing handle without executing.

## TaskFlow signal-routing layer

`TaskFlow::signal(from, to, signalType, affinity)` accepts the same
`ThreadAffinity` enum but **re-interprets some values** at the
signal-edge level rather than handing them straight to
`IThreadManager`. In particular, `Any` here means "run the receiver
synchronously on the emitter's thread, with no `IThreadManager`
involvement" — a local override of the generic "engine picks a fast
worker" meaning. Any non-`Any`, non-`Named` value wraps the receiver in
an adapter that re-posts the handler through
`IThreadManager::schedule(..., affinity)` so the emitter thread does
not block on the handler. `Named` returns an error here too — same
reason as the kernel: the API has no slot to carry a `NamedThreadId`.

The full dispatch contract for the signal-routing layer lands in the
forthcoming [`doc/messaging/signal.md`](../messaging/signal.md).

## Forward links

- [`overview.md`](overview.md) — overview of threading.
- [`parallel-for.md`](parallel-for.md) — `parallelFor` helper that
  uses `Pool` to fan out + barrier in one call.
- [`fsm-threading.md`](fsm-threading.md) — FSM controller-thread
  invariant and `bindToControllerThread()` contract.

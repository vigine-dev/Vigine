# `parallelFor`

`parallelFor` is the engine's fan-out + barrier helper. It splits
`[0, count)` across the thread pool, dispatches each chunk through
`IThreadManager::schedule`, and returns a single `ITaskHandle` whose
`wait()` blocks until every chunk has finished. It is a free function
rather than a method on `IThreadManager` so the manager interface
stays focused on dispatch primitives — fan-out is a pattern composed
on top.

## Signature

The helper lives in
[`include/vigine/core/threading/parallel_for.h`](../../include/vigine/core/threading/parallel_for.h):

```cpp
namespace vigine::core::threading
{
[[nodiscard]] std::unique_ptr<ITaskHandle>
    parallelFor(IThreadManager                       &tm,
                std::size_t                           count,
                std::function<void(std::size_t index)> body,
                ThreadAffinity                        affinity = ThreadAffinity::Pool);
}
```

The signature is `R-NoTemplates`-compliant: the body is a
`std::function`, not a template parameter, so call sites compile
against the interface header without dragging in the body's full type.

## Behavior

**Chunking.** The helper queries `tm.poolSize()` once and computes
`chunkSize = max(1, count / poolSize)`; the number of chunks is the
ceiling division `(count + chunkSize - 1) / chunkSize`. So `count = 10`
over a pool of `3` produces `4` chunks: `[0,3)`, `[3,6)`, `[6,9)`,
`[9,10)`. The intent is roughly one chunk per worker — saturate the
pool without paying `count` per-index scheduling costs. When
`poolSize()` reports `0` the helper falls back to a single chunk
covering the whole range; the pool drains it serially.

**Per-chunk runnable.** Each chunk is wrapped in an internal
`IRunnable` that loops `for (i = begin; i < end; ++i) body(i)` on its
worker. The `body` callable is shared across chunks via a single
`shared_ptr<std::function>`, so `N` chunks do not each copy-construct
the body. Exceptions thrown out of `body` are caught locally and
turned into an error `Result` for that chunk.

**Aggregate `Result`.** The returned `ITaskHandle` holds a shared
composite that decrements once per chunk completion. The first failing
chunk wins the aggregated error slot — later failures are observed
but their messages are discarded, so the reported `Result` is
deterministic. When every chunk succeeds, `wait()` returns a default-
constructed successful `Result`. `cancel()` flips a flag chunk
runnables check between iterations, so a long chunk can bail out
mid-flight.

## Edge cases

- **`count == 0`.** Fast-path. The helper builds a composite with
  `pending == 0`, marks it settled in the constructor, and returns it
  without dispatching a chunk. `wait()` returns immediately with a
  successful `Result`; `ready()` is `true` from the first call.
- **Chunk failure — first error wins.** If a chunk returns an error
  `Result` (or its body throws), that error is stored the first time
  `notifyChunkDone` sees a failure. The aggregate handle still settles
  only after **every** chunk has reported in, so `wait()` waits for the
  slowest chunk regardless of which one failed first.
- **`ThreadAffinity::Named` is unsupported.** `affinity` is passed
  straight to `IThreadManager::schedule`, which rejects `Named` because
  the generic call cannot carry a `NamedThreadId`. Every chunk's
  schedule returns an error handle, the runnables are destroyed
  without running, and the destructor fallback settles each pending
  count with an error `Result`. Use `Pool` (default) or `Any`; for
  fan-out onto one named thread, submit a single runnable through
  `scheduleOnNamed` instead.

## Example

Apply a per-entity update across an array of components:

```cpp
#include "vigine/core/threading/parallel_for.h"

using namespace vigine::core::threading;

void advanceParticles(IThreadManager &tm, std::span<Particle> ps, float dt)
{
    auto handle = parallelFor(tm, ps.size(),
        [ps, dt](std::size_t i) { ps[i].advance(dt); });

    if (auto r = handle->wait(); r.isError())
    {
        logError("particle update failed: {}", r.message());
    }
}
```

The captured `span` and `dt` must outlive the returned handle —
`parallelFor` does not deep-copy caller state.

## Cross-links

- [`overview.md`](overview.md) — four-category thread model that
  `parallelFor` builds on top of.
- [`thread-affinity.md`](thread-affinity.md) — per-affinity dispatch
  semantics, including why `Named` cannot ride the generic schedule
  path.
- FSM safety note — preserve the controller-thread invariant; bodies
  dispatched from a pool worker must not poke FSM state.

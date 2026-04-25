# fanout_fsm

Demonstrates that `vigine::core::threading::parallelFor` can fan out a
range of work items across the engine thread pool and that each
dispatched body can drive its own independent `IStateMachine` through
a small state cycle without contending with the others.

The demo wires three primitives:

  * `IThreadManager` -- default-constructed config; the factory
    derives the pool size from `std::thread::hardware_concurrency()`.
  * `parallelFor(tm, N, body)` -- splits `[0, N)` into chunks sized so
    each pool worker pulls roughly one chunk; returns one aggregated
    `ITaskHandle` whose `waitFor()` blocks until every chunk has
    settled and propagates the first chunk error if any.
  * `IStateMachine` -- one per `parallelFor` body. Each body calls
    `statemachine::createStateMachine`, registers a `Working` and a
    `Done` state, walks the FSM through `Idle` (the auto-provisioned
    default) -> `Working` -> `Done`, and records its completion bit
    in a per-index atomic slot.

After the aggregated handle waits successfully, `main` aggregates the
per-body completion bits and prints `fanout completed: X/N FSMs
reached final state`.

## Why each body owns its FSM

`example/parallel_fsm` shows the pattern for a pair of FSMs that share
state and ping-pong through a shared bus -- there each FSM is bound to
its own named thread via `bindToControllerThread` and every transition
runs on that thread. The fanout demo is the opposite shape: `N`
independent workflows running in parallel with no shared FSM. Keeping
each FSM private to its `parallelFor` body means there is nothing for
the bodies to race on, so the FSM is left unbound; the
`AbstractStateMachine::checkThreadAffinity` gate is intentionally
inactive in that mode and sync `transition()` runs on whichever pool
worker happens to pick the chunk up.

## Build

The example is opt-in: the engine's top-level `CMakeLists.txt` only
includes `example/fanout_fsm/CMakeLists.txt` when
`BUILD_EXAMPLE_FANOUT_FSM` is `ON` (default `OFF`).

```bash
cmake -B build -DBUILD_EXAMPLE_FANOUT_FSM=ON
cmake --build build --target example-fanout-fsm
./build/bin/example-fanout-fsm
```

On Windows / MSVC the executable lands in `build/bin/Debug/` (or
`build/bin/Release/`) depending on the configuration; adjust the run
command accordingly.

## Expected output

```
fanout completed: 16/16 FSMs reached final state
```

Exit code `0` on success. A non-zero exit code means at least one body
failed to walk its FSM through the full cycle, the aggregated
`parallelFor` handle reported an error, or the bounded wait timed out
-- the example prints the partial completion count so a regression is
observable from the log alone.

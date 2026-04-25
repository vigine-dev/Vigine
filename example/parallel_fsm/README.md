# parallel_fsm

Demonstrates two state machines, each pinned to its own engine-managed
named thread, exchanging Ping <-> Pong signals through a shared
`IMessageBus`.

The example wires the three primitives an engine application typically
combines:

  * `IThreadManager` registers two named threads (`fsm_a`, `fsm_b`) and
    serialises every transition for each FSM onto its dedicated thread.
  * `IStateMachine` is bound to its named thread via
    `bindToControllerThread`, so every sync `transition()` call lives on
    that thread and the controller-affinity contract holds (the assert
    in `AbstractStateMachine::checkThreadAffinity` stays silent).
  * `IMessageBus` carries the Ping / Pong signal pair between the two
    sides. Each subscriber runs on the publisher's thread (Shared
    policy), so the subscriber never calls `transition()` itself --
    instead it schedules a tick runnable on the peer's named thread,
    which performs the active -> waiting -> active transition pair and
    posts the response.

Note that the example uses synchronous `transition()` exclusively. The
asynchronous `requestTransition` / `processQueuedTransitions` API is
deliberately out of scope for this example.

## Build

The example is opt-in: the engine's top-level `CMakeLists.txt` only
includes `example/parallel_fsm/CMakeLists.txt` when
`BUILD_EXAMPLE_PARALLEL_FSM` is `ON`.

```bash
cmake -B build -DBUILD_EXAMPLE_PARALLEL_FSM=ON
cmake --build build
./build/bin/example-parallel-fsm
```

On Windows / MSVC the executable lands in `build/bin/Debug/` (or
`build/bin/Release/`) depending on the configuration; adjust the run
command accordingly.

## Expected output

```
exchanges: 100/100
```

Exit code `0` on success. A non-zero exit code means the cycle did not
reach the configured exchange target within the safety-belt deadline,
or a transition / post call returned an error result -- the example
prints the partial counter so a regression is observable from the log
alone.

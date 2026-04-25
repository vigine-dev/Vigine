# threaded_bus

Demonstrates that `IMessageBus` delivers messages from N publisher
threads to a single subscriber on the receiving slot **without ever
invoking `onMessage()` concurrently for that subscriber** (the
per-subscriber serialisation contract documented on `IMessageBus::post`).

The example runs eight publishers on the worker pool, each posting 100
messages back-to-back to a shared-policy bus. A single `CountingSubscriber`
maintains an in-flight atomic counter; the property being verified is
that the counter rises to one on entry to each `onMessage()` call and
never exceeds one. If the bus ever broke the serialisation contract,
two threads would observe each other's increments and the demo would
report a non-zero `Reentry violations` line. With the contract intact
the violation counter stays at zero across all 800 dispatches and the
program exits with status 0.

## Build

The example is opt-in: the engine's top-level `CMakeLists.txt` only
includes `example/threaded_bus/CMakeLists.txt` when
`BUILD_EXAMPLE_THREADED_BUS` is `ON`.

```bash
cmake -B build -DBUILD_EXAMPLE_THREADED_BUS=ON
cmake --build build
./build/bin/example-threaded-bus
```

On Windows / MSVC the executable lands in `build/bin/Debug/` (or
`build/bin/Release/`) depending on the configuration; adjust the run
command accordingly.

## Expected output

```
Received: 800 / 800
Reentry violations: 0
```

Exit code 0 on success. A non-zero exit code means either a publisher
reported an error `Result`, the dispatch count drifted from the expected
total, or the subscriber observed a re-entrant `onMessage` call.

# Vigine

Vigine is a C++20 game-engine substrate built around a graph-based core,
an explicit message bus, a finite-state-machine controller, and an ECS
data layer. It targets Windows, Linux, and macOS through a single CMake
project, and is currently pre-1.0 — the first tagged release, `v0.1.0`,
is being prepared under issue #197.

## Quickstart

See [`doc/quickstart.md`](doc/quickstart.md) for the minimal end-to-end
path from clone to a running example on Windows, Linux, and macOS.
CMake presets for each tier-1 platform ship in `CMakePresets.json`.

## Examples

- [`example/window/`](example/window/) — Win32 window with mouse and
  keyboard signal routing into the engine bus. Builds on Linux and macOS
  when the Vulkan SDK is present; otherwise the target is skipped at
  configure time and the core library still builds.
- [`example/postgresql/`](example/postgresql/) — minimal `libpqxx` round
  trip, illustrating the storage-layer integration path.

### Example window controls

For the `example-window` application:

- `W`/`S`/`A`/`D` — move camera on horizontal plane.
- `Q`/`E` — move camera down/up.
- `Shift` (hold) — sprint multiplier.
- Left mouse button + move — camera look (yaw / pitch).
- Mouse wheel — move camera along current view direction.

Input flow and signal routing are documented in
[`doc/sequence-window-signal.md`](doc/sequence-window-signal.md).

## Threading model

Vigine's FSM controller runs on a single controlling thread; heavy work
fans out into a thread pool through `IThreadManager::schedule` and
`scheduleOnNamed`. Signal routing respects `ThreadAffinity` so
subscribers receive messages on the thread they declared. Detailed
docs under `doc/threading/` land in follow-up PRs.

## Versioning

Vigine follows [SemVer](https://semver.org). Pre-1.0 minor bumps may
carry breaking changes. See [`doc/versioning.md`](doc/versioning.md)
for the full policy, release-branch contract, and experimental-tier
scope. Release notes live in [`CHANGELOG.md`](CHANGELOG.md).

## Building

The full build baseline — compiler flags, CI matrix, sanitizer notes,
and the Vulkan fallback policy — is documented in
[`doc/build.md`](doc/build.md). For a first build, the quickstart is
the fastest path.

## License

MIT — see [`LICENSE`](LICENSE) at repo root.

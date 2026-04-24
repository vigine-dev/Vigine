# Quickstart

Vigine is a data-driven game engine: a task-flow over a message bus with
a pluggable platform / render / storage layer. This page walks from a
fresh clone to a running window example in under ten minutes.

## Prerequisites

| OS          | Compiler                 | CMake | Vulkan SDK (for `example/window`) |
|-------------|--------------------------|-------|-----------------------------------|
| Windows 10+ | MSVC 2022 (17.5 / VS 18+)| 3.25+ | 1.3 (e.g. 1.4.341.1)              |
| Ubuntu 22+  | GCC 13+ or Clang 17+     | 3.25+ | 1.3 (LunarG or distro package)    |
| macOS 13+   | AppleClang 15+ (Xcode 15)| 3.25+ | 1.3 (MoltenVK via LunarG SDK)     |

Git and a vcpkg-compatible network connection are also required — vcpkg
runs in manifest mode and fetches dependencies on the first configure.

The Vulkan SDK is optional for the core library, but required to link
and run `example/window`. On Linux and macOS, the example still builds
if Vulkan 1.2 is the only version available — CMake emits a visible
warning and falls back. If no Vulkan SDK is present at all, the window
example skips cleanly instead of failing the whole build.

## Clone

```text
$ git clone https://github.com/vigine-dev/Vigine.git
$ cd Vigine
```

## Configure and build

The repository ships CMake presets for each tier-1 platform. Pick the
one matching your host.

### Windows

```text
$ cmake --preset windows-debug
$ cmake --build --preset windows-debug
```

### Linux

```text
$ cmake --preset linux-debug
$ cmake --build --preset linux-debug
```

### macOS

```text
$ cmake --preset macos-debug
$ cmake --build --preset macos-debug
```

### Preset-free fallback

If your toolchain does not see `CMakePresets.json`, the generic form
works on every platform once `VULKAN_SDK` and `CMAKE_TOOLCHAIN_FILE`
point at your vcpkg install:

```text
$ cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
$ cmake --build build --config Debug
```

## Run the window example

The window example binary is named `example-window` and lands under
`build/bin/` after a successful build.

```text
$ ./build/bin/example-window          # Linux / macOS
$ build\bin\Debug\example-window.exe  # Windows (multi-config generators)
```

Controls are documented in the root `README.md` under "Example Window
Controls". If the Vulkan SDK is missing on Linux or macOS, the example
target is skipped at configure time — the core library still builds so
you can keep iterating on engine code.

## Next steps

- `doc/README.md` — diagrams and a high-level tour of the engine layout.
- `doc/architecture.md` — subsystem boundaries and the layering rules.
- `doc/engine_lifecycle.md` — how an engine starts, ticks, and shuts down.
- `doc/build.md` — full build baseline: compiler flags, CI matrix,
  sanitizer notes, Vulkan fallback policy.

Once the engine runs, the other `example/` subdirectories
(`threaded_bus`, `parallel_fsm`, `fanout_fsm`) exercise the messaging
and threading features individually — each is wired into the same
top-level CMake project, so the commands above build them all.

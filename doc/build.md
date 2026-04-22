# Building Vigine

Vigine compiles under a single codified baseline: CMake 3.25, C++23
(no GNU extensions), Windows 10+, Vulkan 1.3. The same CMake project
drives Windows, Linux, and macOS builds. CI verifies every
combination on every push.

## Required toolchain

| OS | Compiler | CMake | Vulkan SDK |
|----|----------|-------|------------|
| Windows 10+ | MSVC 2022 (17.5+) | 3.25 | 1.3 |
| Ubuntu 22.04+ | g++-13 or clang-16 | 3.25 | 1.3 |
| macOS 13+ | AppleClang 15+ (Xcode 15) | 3.25 | 1.3 |

The CMake baseline is pinned by `cmake_minimum_required(VERSION 3.25)`
at the top of the root `CMakeLists.txt`. The C++ standard is pinned by
`CMAKE_CXX_STANDARD 23` together with `CMAKE_CXX_EXTENSIONS OFF`
(no `-std=gnu++23`).

## Windows baseline defines

The root CMake adds these on every WIN32 build:

```
_WIN32_WINNT = 0x0A00          # Windows 10
NTDDI_VERSION = 0x0A000000     # NTDDI_WIN10
WIN32_LEAN_AND_MEAN            # trim <Windows.h>
NOMINMAX                       # avoid min/max macro collisions
VK_USE_PLATFORM_WIN32_KHR      # surface-factory selector
```

If a newer API surface is needed, bump `_WIN32_WINNT` and
`NTDDI_VERSION` together; they are paired intentionally.

## Warning level

`cmake/VigineCompileOptions.cmake` applies per-toolchain warning
flags to the `vigine` library target (scoped, so vendored FreeType
does not inherit them):

| Toolchain | Flags |
|-----------|-------|
| MSVC / clang-cl | `/W4 /permissive- /Zc:__cplusplus` |
| GCC / Clang (any non-MSVC) | `-Wall -Wextra -Wpedantic` |

`/WX` and `-Werror` are intentionally not enabled in this baseline; a
follow-up change will flip them once the pre-existing render-header
warnings are cleaned up.

## Local build

```bash
cmake -S . -B build
cmake --build build --config Debug
cmake --build build --config Release
```

On Windows, CMake selects the Visual Studio 2022 generator
automatically; pass `-G Ninja` if Ninja is installed and preferred.

## CI matrix

`.github/workflows/ci.yml` defines a three-OS build matrix:

| Runner | Configs |
|--------|---------|
| `windows-2022` | Debug, Release |
| `ubuntu-22.04` | Debug, Release |
| `macos-13` | Debug, Release |

Each cell caches vcpkg keyed on the OS and the SHA of `vcpkg.json`, so
a lockfile bump invalidates only the affected slices. The Python
static checkers (`check_graph_purity`, `check_naming_convention`,
their pytest suites) continue to run on `ubuntu-latest` only.

## Vulkan 1.3 fallback

The root CMake tries `find_package(Vulkan 1.3 REQUIRED)`. If only
Vulkan 1.2 is present on a build host, configuration emits a visible
`WARNING` and falls back to 1.2 so local development is not blocked
on every engineer's laptop. CI runners always pin 1.3.

## Sanitizers

Sanitizer builds (ASAN / UBSAN / TSAN) are not part of this baseline.
They ship in a later change under a dedicated sanitizer-matrix job
that extends the CI matrix above.

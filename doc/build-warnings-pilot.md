# Build warnings pilot — pre-Werror snapshot

Snapshot of compiler warnings on the umbrella branch
`feature/#197-v0.1.0-readiness` before flipping `/WX` (MSVC) or
`-Werror` (GCC/Clang). Used to size the warnings-cleanup leaf
that comes before E.2.

## Methodology

The static library target `vigine` was compiled from the worktree at
`feature/#230-warnings-pilot` (which tracks `feature/#197-v0.1.0-readiness`)
with the warning flags currently active in
`cmake/VigineCompileOptions.cmake`:

- MSVC / clang-cl: `/W4 /permissive- /Zc:__cplusplus`
- GCC / Clang: `-Wall -Wextra -Wpedantic`

`/WX` and `-Werror` are deliberately absent today — that is the switch
the next leaf will flip after the cleanup.

Configure command (Windows host):

```text
cmake -S . -B <build-dir> -G Ninja -DCMAKE_BUILD_TYPE=<Debug|Release> \
      -DENABLE_POSTGRESQL=OFF -DENABLE_EXAMPLE=OFF -DENABLE_UNITTEST=OFF
```

Notes:

- `ENABLE_POSTGRESQL=OFF` skipped the `libpqxx` vcpkg dependency. The
  postgres component lives in the same warning regime as the rest of the
  library, so dropping it does not hide a dedicated noise category.
- `ENABLE_EXAMPLE=OFF` and `ENABLE_UNITTEST=OFF` keep the snapshot
  scoped to the engine library proper. CI will exercise example +
  test trees on top of the same warning policy.
- The build skipped vcpkg manifest-mode install because the vcpkg
  bundled with VS 2026 / Insiders v18 does not yet detect the v18
  install layout on this host (`Unable to find a valid Visual Studio
  instance`). All warnings reported below come from the vigine target
  and the vendored FreeType build script — both of which are in scope
  for the cleanup work; the suppressed dependency only covered libpqxx.
- Warning instances are counted as raw lines emitted by the compiler.
  A header included by N translation units inflates the per-cell count
  by N for every site reported in the header. The "distinct sites"
  counts in the per-cell detail collapse those duplicates.

## Build matrix

| Cell                       | Total warnings | Top categories                       | Top files                                              |
|----------------------------|---------------:|--------------------------------------|--------------------------------------------------------|
| Windows MSVC Debug         |             15 | C4267 (8) / C4100 (6) / C4005 (1)    | meshcomponent.h, vulkanapi.cpp, rendercomponent.cpp    |
| Windows MSVC Release       |             15 | C4267 (8) / C4100 (6) / C4005 (1)    | meshcomponent.h, vulkanapi.cpp, rendercomponent.cpp    |
| Linux GCC Debug            |         TBD-CI | -                                    | -                                                      |
| Linux GCC Release          |         TBD-CI | -                                    | -                                                      |
| Linux Clang Debug          |         TBD-CI | -                                    | -                                                      |
| Linux Clang Release        |         TBD-CI | -                                    | -                                                      |
| macOS Apple Clang Debug    |         TBD-CI | -                                    | -                                                      |
| macOS Apple Clang Release  |         TBD-CI | -                                    | -                                                      |

The TBD-CI rows are filled in once the matrix CI job for this PR runs
on Linux + macOS runners — this Windows host does not have those
toolchains. The MSVC Debug and Release cells produced an identical
warning set: same 15 raw instances, same 8 distinct sites.

## Detail per cell

### Windows MSVC Debug

Compiler: MSVC `19.50.35722` (Visual Studio v18 / VC tools `14.50.35717`)
Generator: Ninja, CMake 4.1, Windows SDK `10.0.26100.0`.
Build steps: 158, all linked successfully (`vigine.lib` produced).

Distinct warning sites (path normalised, instance-count = how many TUs
surface the same header line):

| File                                        | Line | Code   | Description                                          | Instances |
|---------------------------------------------|-----:|--------|------------------------------------------------------|----------:|
| `include/vigine/ecs/render/meshcomponent.h` |   52 | C4267  | `'return': conversion from 'size_t' to 'uint32_t'`   |         4 |
| `include/vigine/ecs/render/meshcomponent.h` |   53 | C4267  | `'return': conversion from 'size_t' to 'uint32_t'`   |         4 |
| `src/ecs/render/rendercomponent.cpp`        |  166 | C4100  | `'pixelSize': unreferenced parameter`                |         1 |
| `src/ecs/render/vulkanapi.cpp`              |  271 | C4100  | `'desc': unreferenced parameter`                     |         1 |
| `src/ecs/render/vulkanapi.cpp`              |  296 | C4100  | `'desc': unreferenced parameter`                     |         1 |
| `src/ecs/render/vulkanapi.cpp`              |  304 | C4100  | `'size': unreferenced parameter`                     |         1 |
| `src/ecs/render/vulkanapi.cpp`              |  304 | C4100  | `'data': unreferenced parameter`                     |         1 |
| `src/ecs/render/vulkanapi.cpp`              |  304 | C4100  | `'handle': unreferenced parameter`                   |         1 |
| `external/freetype/builds/windows/ftsystem.c` | 29 | C4005  | `'WIN32_LEAN_AND_MEAN': macro redefinition`          |         1 |

The two `meshcomponent.h` lines amplify by a factor of four because
four translation units include the header and instantiate the
warning: `meshcomponent.cpp`, `rendercomponent.cpp`, `rendersystem.cpp`,
`graphicsservice.cpp`. Fixing the inline definitions in the header
collapses those eight raw instances at once.

The five C4100 sites in `vulkanapi.cpp` are all stub method bodies
(`submitDrawCall`, `createBuffer`, `uploadBuffer`) whose source comments
explicitly mark them as future-phase implementations. The single
C4100 in `rendercomponent.cpp:166` is the same shape (an unused
`pixelSize` parameter in `ensureGlyph`).

The lone C4005 lives in vendored FreeType (`ftsystem.c`): the top
`CMakeLists.txt` predefines `WIN32_LEAN_AND_MEAN` for the whole tree
(`add_compile_definitions(WIN32_LEAN_AND_MEAN ...)`), and FreeType then
re-defines it. FreeType is bundled via `add_subdirectory(... EXCLUDE_FROM_ALL)`
and is *not* covered by `vigine_apply_compile_options`, so this leaks
straight from the vendored target's own `/W4` config.

### Windows MSVC Release

Identical warning set to Debug — same 15 raw instances at the same
file-line-code triples. No optimization-level-specific warnings
(no C4701 / C4702 / C4703 etc. surfaced under `/O2`). The build
completed and produced `vigine.lib` as a Release static library.

The MSVC `/Wp64` portability set, debug-only `/RTC*` runtime checks,
and Release-only optimizer warnings (e.g. uninitialized-use, dead
stores) all came back clean against the current code.

### Linux GCC / Linux Clang / macOS Apple Clang

Not measured on this Windows host — see "TBD-CI" rows above. Expected
deltas (informed reading, to be confirmed by the CI run on this PR):

- GCC + Clang will surface the same `meshcomponent.h` size_t→uint32_t
  conversion under `-Wconversion`-adjacent diagnostics in `-Wextra`
  (likely `-Wsign-compare` or `-Wnarrowing` depending on context),
  though `-Wall -Wextra -Wpedantic` without explicit `-Wconversion`
  may *not* report it — to be confirmed.
- Unreferenced parameters under `-Wunused-parameter` (part of
  `-Wextra`) will surface the five vulkanapi.cpp stubs and the one
  rendercomponent.cpp stub identically.
- Macro redefinition will appear under `-Wmacro-redefined`
  (Clang) / built-in (GCC) — same shape as MSVC C4005.
- Apple Clang on macOS may add Objective-C++ warnings from
  `cocoawindowcomponent.mm` and `metalsurfacefactory.cpp`. None of
  those translation units compile on Windows, so the Windows snapshot
  is silent on that surface.
- macOS / Linux builds also exercise the XCB and Cocoa platform
  branches that are entirely skipped on Windows. Any warnings in
  those branches are CI-only signal.

## Recommendation

The Windows snapshot reports 15 raw instances and 8 distinct sites.
This is small enough that a focused cleanup leaf can fully clear the
in-house code, leaving only the FreeType macro-redefinition pattern
that needs a different strategy.

Categorisation and proposed action item per category:

1. **Real bugs / lurking-narrowing — fix in code (1 cleanup file).**
   - `meshcomponent.h:52, 53` — `getVertexCount()` / `getIndexCount()`
     return a narrowed `uint32_t` from `_vertices.size()` /
     `_indices.size()` (which are `std::vector::size_type`, i.e.
     `size_t`). On 64-bit MSVC this is a real truncation site if
     a mesh ever grows past `UINT32_MAX` vertices or indices. Fix:
     either change the return type to `std::size_t` (preferred for
     symmetry with `std::vector`) or wrap with
     `static_cast<uint32_t>(...)` plus a `constexpr` bound check /
     `assert`. Touching the header collapses 8 of the 15 raw
     instances at once.
   - This is the only "real bug" category in the current snapshot.

2. **Pedantic noise from intentional stubs — silence with `[[maybe_unused]]`
   or named-but-unused convention.** Six instances total, all in
   placeholder methods that the source comments explicitly mark as
   future-phase scaffolding:
   - `vulkanapi.cpp:271` `submitDrawCall(const DrawCallDesc &desc)`
   - `vulkanapi.cpp:296` `createPipeline(const PipelineDesc &desc)`
     — sibling of the bug-shaped warning; this one keeps the
     parameter for the `_vulkanPipelineStore->createPipeline(desc, ...)`
     call inside the body, but the warning fires for the unused
     branch when `_vulkanPipelineStore` is null. Re-check on the
     audit pass.
   - `vulkanapi.cpp:304` `uploadBuffer(BufferHandle handle, const void *data, size_t size)`
     — three parameters, all unused in the stub body.
   - `rendercomponent.cpp:166` `ensureGlyph(... uint32_t pixelSize)`
     — `pixelSize` is never read in the body.

   Recommended fix: `[[maybe_unused]]` on the offending parameters in
   the C++23 source. Add a `// TODO(<issue-ref>)` line wherever the
   parameter is genuinely going to be wired up later. This silences
   MSVC / GCC / Clang uniformly without a per-compiler `#pragma`.
   *(Avoid `(void)param;` casts; the project standard prefers attribute
   syntax in C++23 code.)*

3. **External / vendored dep noise — header-include strategy.**
   One instance, in `external/freetype/builds/windows/ftsystem.c`:
   `WIN32_LEAN_AND_MEAN` is defined twice — once by Vigine's top-level
   CMakeLists and once by FreeType. Two viable fixes:
   1. Remove `WIN32_LEAN_AND_MEAN` from the top-level
      `add_compile_definitions(...)` block. FreeType already defines
      it for its own translation units; the rest of Vigine's TUs do
      not need it on the command line because every Vigine source that
      includes `<windows.h>` should be doing so through a thin
      forwarding header that defines the macro privately.
   2. Or: scope the top-level definition to the `vigine` target only,
      with `target_compile_definitions(vigine PRIVATE WIN32_LEAN_AND_MEAN)`.
      That keeps the macro out of the `freetype` target's compile
      command line, which is the actual conflict surface.

   Either fix is local to `CMakeLists.txt` (no source patches needed)
   and clears the warning for `freetype`. The cleanup leaf should
   prefer option 2 — it preserves the existing semantics for Vigine
   sources without requiring a new forwarding header.

### Sizing

- 8 distinct in-house sites + 1 vendored-dep site = 9 actions.
- Estimated work: half a day of focused editing + one local build to
  confirm the count drops to zero on Windows. CI matrix on the
  cleanup PR confirms Linux GCC / Linux Clang / macOS Apple Clang
  are also clean before E.2 flips `/WX` and `-Werror` globally.
- No suppressions required for in-house code; every site has a clean
  fix in source. Only the FreeType macro redefinition needs CMake
  scoping.

### What this snapshot does not cover

- Unit-test sources (`ENABLE_UNITTEST` was OFF). The `test/` tree
  is also subject to `vigine_apply_compile_options` (or equivalent),
  so the same warning policy will apply. The cleanup leaf must
  re-build with `-DENABLE_UNITTEST=ON` to capture any test-only
  warnings before flipping `/WX`.
- Example projects (`ENABLE_EXAMPLE` was OFF). Same caveat.
- Postgres component (`ENABLE_POSTGRESQL` was OFF on this host —
  the host's vcpkg cannot install `libpqxx` against VS v18). The
  CI matrix run for this PR exercises postgres; any extra warnings
  will surface there and need to fold into the same cleanup leaf.
- Sanitizer builds (`VIGINE_SANITIZER` empty). Sanitizer builds
  add `-fsanitize=...` but inherit the same `-Wall -Wextra -Wpedantic`,
  so warning counts should match the regular Linux Clang Debug cell.

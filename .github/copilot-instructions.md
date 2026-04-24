# Copilot Instructions for Vigine

## General Rules
- Keep changes minimal and task-focused.
- Preserve existing public APIs unless the task requires API changes.
- After edits, check compile/lint errors for changed files.
- Prefer small, targeted patches over large rewrites.

## Language Guidance
- English only for code, code comments, doxygen, commit messages, issue
  and pull-request titles/bodies, and anything under `doc/`.
- Internal orchestration files that are never committed to this repository
  may use Ukrainian; everything checked in stays English.

## Architecture Reference
- For the three-layer source tree (`core/` + `api/` + `impl/`) overview,
  see `doc/architecture.md` (lands in a follow-up PR). Treat this as the
  forward reference for structural conventions.

## Cross-Platform Expectations
- Vigine targets Windows, Linux, and macOS; write cross-platform code by
  default.
- Keep core behavior platform-agnostic and expressed via interfaces and
  abstractions.
- Place OS-specific code in dedicated platform folders and classes
  (for example WinAPI / X11 / Cocoa layers).
- Platform-specific classes must implement shared interfaces and must not
  leak OS types into generic layers.
- You do not need to implement every platform at once, but new code must
  leave extension to other platforms straightforward.

## File-Name Conventions
- One public class per header, named exactly after the class:
  `ClassName.h` / `ClassName.cpp` (bare, no prefix, no suffix).
- Pure-virtual interfaces use the `I` prefix: `IRenderer.h`, `ISystem.h`.
- Abstract base classes with at least one implemented method use the
  `Abstract` prefix: `AbstractService.h`, `AbstractTask.h`.
- Match the on-disk file name to the class name exactly, including case.

## Search Strategy
- Prefer `rg --files` to discover files and `rg "pattern" path/` for
  content search; avoid reading whole files when a targeted match works.
- Search from the workspace root first, then narrow by folder.
- Exclude vendor trees unless the task requires them:
  - `external/`
  - `build/`
  - `vcpkg_installed/`
- For C++ symbol lookups, prioritize these folders:
  - `include/vigine/`
  - `src/`
  - `example/`
  - `test/`
- Inspect both the declaration and the implementation when chasing a
  symbol. Narrow noisy searches with glob filters, for example:
  - `rg "WindowEventHandler" include src example`
  - `rg --files | rg "winapi|window|event"`

## Geometry and Shader Isolation
- When implementing a new geometric UI or scene element (cube, pyramid,
  panel, focus frame, gizmo, helper shape, etc.), keep it independent of
  unrelated elements.
- Each new element owns its own geometry definition and its own shader
  pair (vertex + fragment), even if another existing shader looks similar.
- Do not reuse a text/editor shader for non-text elements and do not
  reuse panel/editor shaders for focus-frame effects.
- Keep visual behavior and animation parameters local to the element's
  own shader pipeline to avoid cross-feature coupling.

## Before Editing
- Read the target file and its immediate dependencies first.
- Read the relevant documentation under `doc/` before changing class
  relationships, signals, or task flows.
- Check whether related files changed recently before patching.
- Do not revert user changes unless explicitly requested.

## Documentation Sync
- When changing classes, inheritance, ownership, or interactions between
  engine parts, update the relevant documentation under `doc/` in the
  same change set.
- Keep `doc/README.md` and affected Mermaid diagrams in sync with the
  code.
- When a new subsystem, folder, or layer is introduced, document where
  it lives and how it relates to existing classes.

## Communication Style
- Keep progress updates short and practical.
- Explain what changed and why, with file references.

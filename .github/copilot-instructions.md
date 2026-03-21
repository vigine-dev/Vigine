# Copilot Instructions for Vigine

## General Rules
- Keep changes minimal and task-focused.
- Do not refactor unrelated code unless explicitly requested.
- Preserve existing public APIs unless the task requires API changes.
- After edits, check compile/lint errors for changed files.

## Architecture Change Policy
- Do not modify core engine architecture classes unless the user explicitly asks for it.
- Core architecture classes include (at minimum): Context/ContextHolder, TaskFlow/AbstractTask, StateMachine, AbstractService/AbstractSystem contracts, EntityManager, and engine-level ownership/binding flow.
- You may and should add new Components, Systems, and Services when needed to implement requested functionality.
- Prefer extending behavior through existing extension points instead of rewriting engine foundations.
- Do not introduce new global managers, lifecycle models, or cross-cutting ownership changes without explicit approval.
- If a task seems to require edits in core architecture classes, first propose a minimal non-architectural option and ask for confirmation.

## C++ and CMake Conventions
- Keep current coding style and formatting used in nearby files.
- Prefer small, targeted patches over large rewrites.
- Avoid adding platform-specific logic to cross-platform interfaces.

## Cross-Platform Architecture Rule
- Design new features with future multi-OS support in mind (Windows/Linux/macOS).
- Keep core behavior platform-agnostic and expressed via interfaces/abstractions.
- Place OS-specific code in dedicated platform folders and classes (for example, WinAPI/X11/Cocoa layers).
- Platform-specific classes must implement shared interfaces and avoid leaking OS types into generic layers.
- Do not implement all platforms immediately; ensure current changes keep extension to other OSes straightforward.

## File Search Strategy
- Prefer `rg --files` to discover files quickly.
- Prefer `rg "pattern" path/` for content search.
- Search from workspace root first, then narrow by folder.
- Exclude vendor code unless needed:
  - `external/`
  - `build/`
  - `vcpkg_installed/`
- When searching symbols, inspect both declarations and implementations.
- For C++ symbol lookups, prioritize these folders:
  - `include/vigine/`
  - `src/`
  - `example/`
  - `test/`
- If initial search is noisy, narrow with glob filters, for example:
  - `rg "WindowEventHandler" include src example`
  - `rg --files | rg "winapi|window|event"`

## Before Editing
- Read the target file and immediate dependencies first.
- Read the relevant documentation in `doc/` before changing architecture, class relationships, signals, or task flows.
- If the task does not provide a folder structure, inspect the workspace and use `doc/README.md` as the current high-level engine map.
- Confirm whether related files changed recently before patching.
- Do not revert user changes unless explicitly requested.

## Documentation Sync
- When changing classes, inheritance, ownership, or interactions between engine parts, update the relevant documentation in `doc/` in the same task.
- At minimum, keep `doc/README.md` and the affected Mermaid diagrams in sync with code changes.
- If a new subsystem, folder, or architectural layer is introduced, document where it lives and how it relates to existing engine classes.

## Communication Style
- Answer in Ukrainian unless user requests another language.
- Keep progress updates short and practical.
- Explain what changed and why, with file references.


# 🖼️ Plan: Dynamic Image Gallery — All Photos from resource/img/

## 🤖 Agent Requirements

Implementing agent: **Senior C++ Engineer**. Must understand ECS architecture, Vulkan rendering pipeline, `std::filesystem` API, and the project's coding conventions (see `.github/copilot-instructions.md`).

## 📌 TL;DR

Replace hardcoded 3-image lists in `LoadTexturesTask` and `SetupTexturedPlanesTask` with dynamic directory scanning so ALL images from `resource/img/` are loaded and displayed as textured planes. Two files changed, zero engine modifications.

## 🔴 Current State

- `resource/img/` contains **6 images**: ocean.jpg, ocean2.jpg, ocean3.jpg, ocean4.jpg, ocean5.jpg, ocean6.jpg
- `LoadTexturesTask` hardcodes only 3 paths: `{"resource/img/ocean.jpg", "resource/img/ocean2.jpg", "resource/img/ocean3.jpg"}`
- `SetupTexturedPlanesTask` hardcodes 3 `PlaneConfig` entries with fixed positions/rotations
- Descriptor pool supports up to 32 textures — sufficient for current needs
- `<filesystem>` already included in `loadtexturestask.cpp` (used for `std::filesystem::exists`)

---

## 🟢 Phase 1: Dynamic Texture Loading

**⛓️ Dependencies:** none (first phase)

**🎯 Goal:** `LoadTexturesTask` dynamically discovers all images in `resource/img/` instead of using a hardcoded list.

### Steps

**1.1.** In `LoadTexturesTask::execute()`, replace the hardcoded `imagePaths` vector (line 44–45) with a `std::filesystem::directory_iterator` scan of `resource/img/`.
- Filter entries: `is_regular_file()` and extension in {`.jpg`, `.jpeg`, `.png`} (case-insensitive comparison).
- Collect matching paths into `std::vector<std::string>`, then `std::sort()` alphabetically for deterministic entity ordering.
- If directory doesn't exist or is empty, return `Result::Code::Error` with descriptive message.

> ✅ **Verification 1.1:** Build `cmake --build build/`. Confirm no compile errors. Grep the file for any remaining hardcoded `"ocean"` strings — there should be none.

**1.2.** Keep all existing per-image logic unchanged: `ImageLoader::loadImage()`, entity creation (`TextureEntity_N`), `createTextureComponent()`, `uploadTextureToGpu()`. The only change is the source of `imagePaths`.

> ✅ **Verification 1.2:** Run `example-window`. Console output should list **6** "Loaded and uploaded texture N: …" lines (one per image). No errors or warnings.

### 🏁 Phase 1 Verification

- 🔨 `cmake --build build/` — zero errors
- 🖥️ Run `example-window` — console shows 6 loaded textures with correct file paths
- 🧪 Temporarily rename one .jpg → .bak → restart → 5 textures load, no crash
- ↩️ Restore the file

### 📂 Relevant Files

- `example/window/task/vulkan/loadtexturestask.cpp` — lines 44–45: replace `imagePaths` vector with directory scan. Reference: existing `std::filesystem::exists()` usage at line 52. Reference: `ImageLoader::loadImage()` in `example/window/task/vulkan/imageloader.cpp`.

---

## 🟢 Phase 2: Dynamic Gallery Layout

**⛓️ Dependencies:** Phase 1 (texture entities must exist with sequential aliases `TextureEntity_0` … `TextureEntity_N`)

**🎯 Goal:** `SetupTexturedPlanesTask` creates a plane for every loaded texture entity, with auto-computed gallery positions.

### Steps

**2.1.** In `SetupTexturedPlanesTask::execute()`, replace the hardcoded `planes` vector (lines 55–60) with a dynamic discovery loop:
- Iterate aliases `"TextureEntity_0"`, `"TextureEntity_1"`, … via `entityManager->getEntityByAlias()` until it returns `nullptr`.
- Store the count as `size_t textureCount`.

> ✅ **Verification 2.1:** Add a temporary `std::cout << "Found " << textureCount << " texture entities"` and confirm it prints **6** at runtime.

**2.2.** Generate `PlaneConfig` for each found entity with computed layout:
- Spacing: `const float spacing = 3.5f`
- Start X: `float startX = -static_cast<float>(textureCount - 1) * spacing / 2.0f`
- Per-plane position: `{startX + i * spacing, 2.2f, -15.0f}`
- Per-plane Y-rotation (arc effect): when `textureCount > 1`, `rotationY = -12.0f + 24.0f * (static_cast<float>(i) / static_cast<float>(textureCount - 1))`; when `textureCount == 1`, `rotationY = 0.0f`.
- Entity name: `"TexturedPlane" + std::to_string(i)`, texture entity name: `"TextureEntity_" + std::to_string(i)`.

> ✅ **Verification 2.2:** Build `cmake --build build/`. Confirm no compile errors. Grep the file for hardcoded `"TexturedPlane0"`, `"TexturedPlane1"`, `"TexturedPlane2"` — should find none.

**2.3.** Keep all existing per-plane logic unchanged: shader setup (`textured_plane.vert.spv`/`.frag.spv`, `setHasTextureBinding(true)`), mesh setup (`setProceduralInShader(true, 6)`), texture linking (`setTextureHandle`), aspect-ratio scale computation, transform application. Only the source of plane configs changes.

> ✅ **Verification 2.3:** Run `example-window`. All 6 textured planes visible in the scene, evenly spaced, with ocean images rendered (not black).

### 🏁 Phase 2 Verification

- 🔨 `cmake --build build/` — zero errors
- 🖥️ Run `example-window` — 6 planes visible with correct textures, gallery layout centered
- 🧪 Add a 7th .jpg to `resource/img/` → restart → 7 planes appear, layout auto-adjusts
- 🧪 Remove images leaving only 1 → restart → 1 centered plane, no crash
- 🛡️ Empty `resource/img/` → restart → no planes, no crash, error message in console

### 📂 Relevant Files

- `example/window/task/vulkan/setuptexturedplanestask.cpp` — lines 55–60: replace `planes` vector with dynamic generation. Reference: `entityManager->getEntityByAlias()` for entity lookup. Reference: existing `PlaneConfig` struct defined locally (line 48–54) — keep it, just generate instances dynamically.

---

## 💡 Decisions

- Directory scan path: relative `resource/img/` (matches current convention)
- Supported extensions: `.jpg`, `.jpeg`, `.png` (covers stb_image formats)
- Gallery layout: horizontal arc with `spacing=3.5f`, Y=2.2, Z=-15.0, rotation ±12°
- Descriptor pool (32 max) sufficient for up to 32 images — no engine changes
- Scope: only 2 example task files modified, zero engine code changes

# 🔧 Plan: VulkanAPI Refactoring — ECS-driven Graphics Backend

## 📋 TL;DR

Refactor the monolithic `VulkanAPI` class (~2174 lines, two 500–900 line methods) into a set of small, composable classes behind a `GraphicsBackend` interface. Shaders, geometry, textures, and pipelines become ECS components attached to entities. The Vulkan implementation becomes one pluggable backend (DirectX possible later). Camera logic is extracted. All new code is cross-platform by design.

---

## 🔴 Current Problems

- 🚨 `createSwapchain()` is ~900 lines: creates 9 hardcoded pipelines, loads shaders inline, builds geometry
- 🚨 `drawFrame()` is ~500 lines: mixes camera physics, command recording, buffer uploads, draw calls
- ⚠️ Shader paths and vertex counts (36, 6, 768, 18) are hardcoded magic numbers
- ⚠️ Camera state + physics lives inside VulkanAPI
- ⚠️ `ShaderComponent` exists but is unused; `ShaderProfile` enum in `RenderComponent` maps to hardcoded pipelines
- ⚠️ Platform surface creation uses `#ifdef` blocks inside VulkanAPI
- 🚨 No abstraction layer for replacing Vulkan with another API
- ⚠️ SDF atlas management is interleaved with frame rendering

---

## 🟦 Phase 1: Graphics Backend Abstraction

**🎯 Goal:** Create a `GraphicsBackend` interface so VulkanAPI becomes a pluggable component.

### Steps

1. **Create `include/vigine/ecs/render/graphicsbackend.h`** — abstract interface:
   - `initializeDevice(void* nativeWindow)` → bool
   - `resize(uint32_t w, uint32_t h)` → bool
   - `beginFrame()` → bool
   - `endFrame()` → bool
   - `submitDrawCall(const DrawCallDesc&)` — draw a batch
   - `createPipeline(const PipelineDesc&)` → PipelineHandle
   - `destroyPipeline(PipelineHandle)`
   - `createBuffer(const BufferDesc&)` → BufferHandle
   - `uploadBuffer(BufferHandle, const void* data, size_t size)`
   - `destroyBuffer(BufferHandle)`
   - `createTexture(const TextureDesc&)` → TextureHandle
   - `uploadTexture(TextureHandle, const void* pixels, uint32_t w, uint32_t h)`
   - `destroyTexture(TextureHandle)`
   - `createShaderModule(const std::vector<char>& spirv)` → ShaderModuleHandle
   - `destroyShaderModule(ShaderModuleHandle)`
   - `setViewProjection(const glm::mat4&)`
   - `setPushConstants(const PushConstantData&)` — typed struct
   - Pure virtual destructor

2. **Create handle types** in `include/vigine/ecs/render/graphicshandles.h`:
   - `PipelineHandle`, `BufferHandle`, `TextureHandle`, `ShaderModuleHandle` — opaque uint64_t wrappers
   - `PipelineDesc` — shader modules, vertex layout, blend mode, depth mode, topology
   - `BufferDesc` — size, usage (vertex/index/uniform), memory flags
   - `TextureDesc` — width, height, format, sampler params
   - `DrawCallDesc` — pipeline, buffers, push constants, vertex count, instance count
   - `PushConstantData` — viewProjection, animationData, sunDirectionIntensity, lightingParams, modelMatrix

3. **Refactor `VulkanAPI` to implement `GraphicsBackend`**:
   - Keep all Vulkan-specific members private
   - Map handles to internal Vulkan resources via `std::unordered_map<uint64_t, VkResource>`
   - Move platform surface creation to Phase 3

4. **Update `RenderSystem`** to hold `std::unique_ptr<GraphicsBackend>` instead of `std::unique_ptr<VulkanAPI>`

### 📂 Relevant Files
- `include/vigine/ecs/render/vulkanapi.h` — refactor to implement GraphicsBackend
- `include/vigine/ecs/render/rendersystem.h` — change `_vulkanAPI` type
- `src/ecs/render/rendersystem.cpp` — use interface methods

---

## 🟩 Phase 2: Extract Camera

**🎯 Goal:** Move camera state and physics out of VulkanAPI into a standalone `Camera` class.

### Steps

5. **Create `include/vigine/ecs/render/camera.h`** + `src/ecs/render/camera.cpp`:
   - All camera state: `_cameraYaw`, `_cameraPitch`, `_cameraPosition`, `_cameraVelocity`, movement flags, sprint
   - Methods: `beginDrag()`, `updateDrag()`, `endDrag()`, `zoom()`, `update(float dt)`, `viewMatrix()`, `projectionMatrix(float aspect, float near, float far)`, `forward()`, `screenPointToRay()`, `screenPointToRayFromNearPlane()`
   - Move `CameraControlsConfig` into camera.h as a public config struct
   - Camera becomes a component or owned by RenderSystem (not by the graphics backend)

6. **Remove camera code from VulkanAPI**:
   - Remove ~15 camera methods and ~15 camera fields from VulkanAPI
   - RenderSystem computes view/projection from Camera and passes to GraphicsBackend via `setViewProjection()`

7. **Update RenderSystem**:
   - Own a `Camera` instance (or make it a component on the bound entity)
   - Forward camera input (drag, movement, zoom) to Camera
   - In `update()`: call `camera.update(dt)` then pass matrices to backend

### 📂 Relevant Files
- `src/ecs/render/vulkanapi.cpp` — remove camera methods (lines 88–260)
- `include/vigine/ecs/render/vulkanapi.h` — remove camera fields
- `src/ecs/render/rendersystem.cpp` — add Camera usage

---

## 🟨 Phase 3: Platform Surface Factory

**🎯 Goal:** Extract platform-specific surface creation from VulkanAPI.

### Steps

8. **Create `include/vigine/ecs/render/surfacefactory.h`** — interface:
   - `createSurface(vk::Instance, void* nativeHandle)` → `vk::SurfaceKHR`
   - `requiredInstanceExtensions()` → `std::vector<const char*>`

9. **Create platform implementations**:
   - `src/ecs/render/platform/win32surfacefactory.cpp` — extracts Win32 surface code (vulkanapi.cpp lines 472–510)
   - `src/ecs/render/platform/metalsurfacefactory.cpp` — extracts Metal surface code (vulkanapi.cpp lines 515–547)
   - `src/ecs/render/platform/surfacefactory.cpp` — compile-time factory selection via `#ifdef`

10. **Remove `#ifdef` blocks from VulkanAPI::createSurface()** — delegate to injected SurfaceFactory

### 📂 Relevant Files
- `src/ecs/render/vulkanapi.cpp` — lines 466–553 (createSurface)
- `src/ecs/render/vulkanapi.cpp` — lines 359–387 (initializeInstance extensions)

---

## 🟧 Phase 4: Shader & Pipeline as ECS Components

**🎯 Goal:** Make shaders and pipelines data-driven ECS components instead of hardcoded.

### Steps

11. **Extend existing `ShaderComponent`** (`include/vigine/ecs/render/shadercomponent.h`):
   - Add SPIR-V binary cache: `_vertexSpirv`, `_fragmentSpirv` (loaded lazily)
   - Add `loadFromFile(vertPath, fragPath)` — calls `loadBinaryFile()`
   - Add `vertexLayout()` → `VertexLayoutDesc` (binding descriptions, attributes)
   - Add `blendMode()` → enum { Opaque, AlphaBlend }
   - Add `depthWrite()` → bool
   - Add `topology()` → enum { TriangleList, LineList }
   - ShaderModuleHandle caching (backend-assigned after first pipeline build)

12. **Create `include/vigine/ecs/render/pipelinecache.h`** helper class:
   - Owns compiled pipeline handles per ShaderComponent configuration
   - Key: hash of (vertex shader path + fragment shader path + layout + blend + depth)
   - Lazy creation: first draw call triggers `GraphicsBackend::createPipeline()`
   - Recreated when swapchain invalidated (render pass changes)
   - Owned by RenderSystem (not per-entity — shared pipelines)

13. **Remove `ShaderProfile` enum from `RenderComponent`**:
   - Replace with `ShaderComponent*` reference (or direct ownership)
   - Entity setup code in example/ sets shader paths on the component instead of selecting an enum

14. **Remove hardcoded pipeline creation from `createSwapchain()`**:
   - `createSwapchain()` only creates: swapchain, depth image, render pass, framebuffers, command pool, sync primitives
   - Pipeline creation moves to PipelineCache, triggered lazily during first drawFrame that encounters uncompiled shaders

15. **Update `drawFrame()`**:
   - Instead of binding hardcoded pipeline per profile, iterate entities → look up PipelineCache → bind → draw
   - Group entities by pipeline to minimize bind switches

### 📂 Relevant Files
- `include/vigine/ecs/render/shadercomponent.h` — extend
- `include/vigine/ecs/render/rendercomponent.h` — remove ShaderProfile, add ShaderComponent
- `src/ecs/render/vulkanapi.cpp` — lines 770–1381 (9 pipeline creation blocks → remove)
- `src/ecs/render/vulkanapi.cpp` — drawFrame pipeline bind sections
- `example/window/task/` — entity setup code

---

## 🟪 Phase 5: Geometry as ECS Component

**🎯 Goal:** Make geometry a proper component that provides vertex/index data to the backend.

### Steps

16. **Extend existing `MeshComponent`** (`include/vigine/ecs/render/meshcomponent.h`):
   - Add `vertexCount()` to replace hardcoded magic numbers (36, 6, 768, 18)
   - Add `isProceduralInShader()` flag — for shaders that generate geometry (cube.vert, sphere.vert)
   - Add BufferHandle for GPU-uploaded vertex/index buffers (assigned by backend)
   - Add `dirty()` flag for re-upload tracking

17. **Create GPU buffer management in RenderSystem or GraphicsBackend**:
   - On entity bind → if MeshComponent has CPU data → upload to GPU via `GraphicsBackend::createBuffer()` + `uploadBuffer()`
   - Track BufferHandle lifetime per entity
   - On entity unbind → `destroyBuffer()`

18. **Remove hardcoded vertex counts from `drawFrame()`**:
   - `DrawCallDesc` gets vertex count from MeshComponent
   - Procedural-in-shader meshes store only vertex count (no CPU buffer)

### 📂 Relevant Files
- `include/vigine/ecs/render/meshcomponent.h` — extend
- `src/ecs/render/vulkanapi.cpp` — drawFrame entity loops
- `src/ecs/render/rendersystem.cpp` — entity component creation

---

## 🟫 Phase 6: Texture/Image as ECS Component

**🎯 Goal:** SDF atlas and future textures become components, not VulkanAPI internal state.

### Steps

19. **Create `include/vigine/ecs/render/texturecomponent.h`**:
   - CPU pixel data (or reference), width, height, format
   - GPU TextureHandle (assigned by backend after upload)
   - Generation counter for change detection
   - DescriptorSet handle for shader binding

20. **Extract SDF atlas into TextureComponent**:
   - RenderSystem creates a "SDF Atlas" entity with TextureComponent
   - `setSdfGlyphData()` updates the TextureComponent's pixel data
   - `drawFrame()` binds the texture via handle instead of managing Vulkan images directly

21. **Move atlas upload logic from VulkanAPI to GraphicsBackend interface**:
   - `createTexture()` + `uploadTexture()` replace the 100-line inline staging buffer code in `setSdfGlyphData()`

### 📂 Relevant Files
- `src/ecs/render/vulkanapi.cpp` — lines 1475–1653 (setSdfGlyphData atlas upload)
- `include/vigine/ecs/render/vulkanapi.h` — SDF atlas fields (_sdfAtlasImage, etc.)

---

## ⬜ Phase 7: Break Down Remaining Large Methods

**🎯 Goal:** No method exceeds ~100 lines.

### Steps

22. **Split `createSwapchain()` into helper methods** (after Phase 4 removes pipelines):
   - `createSwapchainImages()` — swapchain object + image views
   - `createDepthResources()` — depth image + view
   - `createRenderPass()` — attachments, subpass, dependencies
   - `createFramebuffers()` — per-image framebuffers
   - `createCommandResources()` — command pool + buffers
   - `createSyncPrimitives()` — semaphores + fences
   - `createSwapchain()` orchestrates these in sequence

23. **Split `drawFrame()` into phases**:
   - `updateAnimationState(float dt)` — rotation angles, camera (after Phase 2: removed)
   - `acquireSwapchainImage()` → imageIndex or failure
   - `recordCommandBuffer(uint32_t imageIndex)` — render pass + draw calls
   - `submitAndPresent(uint32_t imageIndex)` — queue submit + present
   - `recordDrawCalls(commandBuffer)` — iterate entities by pipeline, bind, draw

24. **Extract per-swapchain-image buffer management**:
   - Create `VulkanPerImageBuffer` helper (manages the resize+upload+map pattern used by glyph instances and SDF vertex buffers)
   - Replace duplicated buffer management code in drawFrame (~80 lines × 2 usages)

### 📂 Relevant Files
- `src/ecs/render/vulkanapi.cpp` — entire file

---

## 🏁 Phase 8: Update Example Code & Documentation

### Steps

25. **Update `example/window/`** entity setup to use new components:
   - Set `ShaderComponent` with shader file paths instead of `ShaderProfile` enum
   - Set `MeshComponent` with vertex counts for procedural geometry
   - TextureComponent for SDF atlas

26. **Update `doc/README.md`** and relevant Mermaid diagrams:
   - Add GraphicsBackend, Camera, PipelineCache, TextureComponent, SurfaceFactory to class diagrams
   - Update RenderSystem ownership description

27. **Update `doc/core-class-diagram.md`**:
   - New inheritance: `VulkanAPI : GraphicsBackend`
   - New composition: `RenderSystem → GraphicsBackend, Camera, PipelineCache`

---

## ✅ Verification

1. 🔨 **Compile check** after each phase — `cmake --build build/` must succeed
2. 🖥️ **Run `example-window`** after each phase — visual output must match pre-refactoring (golden screenshot comparison)
3. 🧪 **Run existing tests** — `ctest --test-dir build/`
4. 🧩 **Verify interface completeness** — create a stub `NullBackend : GraphicsBackend` that compiles but does nothing (proves interface is sufficient)
5. 🔗 **Verify entity workflow** — in example, create entity → attach ShaderComponent + MeshComponent → see it render; detach → see it disappear
6. 🪟 After Phase 3: verify Windows build still creates surface correctly
7. 📏 After Phase 7: no method in vulkanapi.cpp exceeds ~100 lines (measurable via line count per function)

---

## 💡 Decisions

- **GraphicsBackend is NOT an AbstractSystem/AbstractComponent** — it's a render resource manager owned by RenderSystem. Entities don't attach "a backend" to themselves; they attach shaders, meshes, textures. RenderSystem uses the one active backend.
- **Camera is owned by RenderSystem**, not by VulkanAPI. Camera could later become a component on a "camera entity" but that's out of scope.
- **PipelineCache is RenderSystem-owned**, shared across entities. Same shader pair → same pipeline handle.
- **Existing `ShaderComponent` is extended**, not replaced. Backward compatibility with the existing header path.
- **Phase order matters**: Phase 1 → 2 → 3 can run somewhat independently; Phase 4 depends on Phase 1; Phase 5–6 depend on Phase 4; Phase 7 can start after Phase 4; Phase 8 runs last.
- **DirectX is NOT implemented** — only the abstract interface is created to keep the door open.
- **Scope excludes**: shader hot-reload, material system, render graph, multi-threading, GPU memory allocator (VMA). These are future work.

---

## 🆕 New Files Created

| Path | Purpose |
|------|---------|
| `include/vigine/ecs/render/graphicsbackend.h` | Abstract graphics API interface |
| `include/vigine/ecs/render/graphicshandles.h` | Handle types, descriptors, enums |
| `include/vigine/ecs/render/camera.h` | Camera state + math |
| `src/ecs/render/camera.cpp` | Camera implementation |
| `include/vigine/ecs/render/surfacefactory.h` | Platform surface abstraction |
| `src/ecs/render/platform/win32surfacefactory.cpp` | Win32 surface |
| `src/ecs/render/platform/metalsurfacefactory.cpp` | Metal surface |
| `include/vigine/ecs/render/pipelinecache.h` | Lazy pipeline compilation cache |
| `src/ecs/render/pipelinecache.cpp` | PipelineCache implementation |
| `include/vigine/ecs/render/texturecomponent.h` | Texture ECS component |
| `src/ecs/render/texturecomponent.cpp` | TextureComponent implementation |

## ✏️ Modified Files

| Path | Changes |
|------|---------|
| `include/vigine/ecs/render/vulkanapi.h` | Implement GraphicsBackend, remove camera/surface code |
| `src/ecs/render/vulkanapi.cpp` | Split into small methods, delegate to helpers |
| `include/vigine/ecs/render/rendersystem.h` | Own GraphicsBackend+Camera+PipelineCache |
| `src/ecs/render/rendersystem.cpp` | Use abstractions instead of direct VulkanAPI calls |
| `include/vigine/ecs/render/shadercomponent.h` | Add SPIR-V cache, layout, blend mode |
| `include/vigine/ecs/render/rendercomponent.h` | Remove ShaderProfile, reference ShaderComponent |
| `include/vigine/ecs/render/meshcomponent.h` | Add vertex count, procedural flag, BufferHandle |
| `example/window/task/` | Use new component API for entity setup |
| `CMakeLists.txt` | Add new source files |
| `doc/README.md` | Update architecture description |
| `doc/core-class-diagram.md` | Update class diagram |

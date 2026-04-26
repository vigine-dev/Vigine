# Vigine Project Diagrams

This folder contains project structure visualizations in Mermaid format.

## Engine structure

If a task description does not include the project folder layout, use this section as the quick map of the engine.

### Main folders

- `include/vigine/` public engine API and core abstractions.
- `include/vigine/api/base/` basic shared types and helpers.
- `include/vigine/impl/service/` public service interfaces and implementations exposed through `Context`.
- `include/vigine/api/messaging/` public messaging API: `IMessageBus`, `ISubscriber`, `ISignalEmitter`, `MessageFilter`, the message-envelope contracts, and the abstract bases concrete buses extend.
- `include/vigine/api/messaging/payload/` payload registry primitives (`PayloadTypeId`, `IPayloadRegistry`, `PayloadRange`) and concrete signal payloads (e.g. `StateInvalidatedPayload`).
- `include/vigine/impl/messaging/` concrete final messaging types: `SystemMessageBus`, `SignalEmitter`, `ConnectionToken`.
- `include/vigine/api/ecs/` ECS-facing engine interfaces (entities, components, systems).
- `include/vigine/api/ecs/platform/` platform-agnostic window and input interfaces.
- `include/vigine/api/ecs/graphics/` rendering interfaces and abstractions.
- `include/vigine/impl/ecs/` concrete ECS types (entity, component manager, factory).
- `include/vigine/impl/ecs/platform/` platform window components and dispatch types.
- `include/vigine/impl/ecs/graphics/` concrete rendering components and systems.
- `include/vigine/experimental/ecs/postgresql/` PostgreSQL data and system API.
- `src/` engine implementations for the public headers.
- `src/impl/ecs/platform/` platform window component implementations; currently includes WinAPI and Cocoa.
- `src/impl/ecs/graphics/` rendering implementation (Vulkan backend, render system, pipeline cache).
- `src/experimental/ecs/postgresql/` PostgreSQL implementation details.
- `example/` example applications built on top of the engine.
- `test/` tests for engine components and architecture.

### Core engine classes

- `vigine::engine::IEngine` is the engine front door; `createEngine` returns an owning `unique_ptr<IEngine>` that wraps an `IContext` aggregator.
- `IContext` aggregates `IStateMachine`, `ITaskFlow`, `IECS`, the system message bus, and the thread manager.
- `IStateMachine` owns the state topology (state ids + transitions keyed by `Result::Code`) and a per-state `ITaskFlow` registry.
- `ITaskFlow` stores task ids and runnable handles (`ITask`), routes them by `Result::Code`, and connects tasks via `ISignalEmitter` for a given `PayloadTypeId`.
- `Result` is the shared execution result type for tasks and transitions.
- `EntityManager` is the legacy entity owner used by examples that still walk the `Entity *` surface.
- `Entity` is the base runtime entity type.
- `AbstractTask` carries the bound `IEngineToken` pointer; concrete tasks reach engine subsystems through the token returned by `api()`.
- `vigine::service::AbstractService` is the modern Level-1 service base; `vigine::ecs::AbstractSystem` is the modern system base that carries its `Entity *` binding by composition.

### Inheritance overview

- `AbstractTask : ITask` (holds the bound `IEngineToken *` by composition)
- `vigine::service::AbstractService : IService`
- `vigine::ecs::AbstractSystem : ISystem` (holds `Entity *` by composition)
- `PlatformService : vigine::service::AbstractService`
- `GraphicsService : vigine::service::AbstractService`
- `DatabaseService : vigine::service::AbstractService`
- `WindowSystem : AbstractSystem`
- `WinAPIComponent : WindowComponent` (Windows-only native window implementation)
- `CocoaWindowComponent : WindowComponent` (macOS native window implementation backed by `NSWindow` + `CAMetalLayer`)
- `RenderSystem : AbstractSystem`
- `RenderComponent` holds per-entity rendering state: `ShaderComponent` (shader paths, pipeline config), `MeshComponent`, `TransformComponent`, `TextComponent`, `TextureComponent`.
- `ShaderComponent` defines per-entity pipeline configuration: SPIR-V shader paths, vertex layout, blend mode, depth flags, topology, instanced rendering, procedural vertex count.
- `MeshComponent` provides vertex/index data, procedural mesh flags, and GPU buffer handles.
- `TextureComponent` stores CPU pixel data, dimensions, format, GPU `TextureHandle`, and descriptor set binding info; used for SDF atlases and future texture-based rendering.
- `PipelineCache` (owned by `RenderSystem`) maps `ShaderComponent` configs to `PipelineHandle` via hash-based caching; creates Vulkan pipelines on first use.
- `EntityDrawGroup` groups entities sharing the same pipeline for draw-call submission.
- `IGraphicsBackend` is the abstract rendering API interface; `VulkanAPI` is the concrete backend.
- `VulkanAPI` is a thin orchestrator (~380 lines); all Vulkan-specific work is delegated to five internal helper classes:
  - `VulkanDevice` (`src/impl/ecs/graphics/`) — Vulkan instance, physical device, logical device, surface, and queues. Also wraps `SurfaceFactory` for platform-specific surface creation.
  - `VulkanSwapchain` (`src/impl/ecs/graphics/`) — swapchain lifecycle, render pass, framebuffers, command pool, sync primitives (semaphores + fences), and per-frame acquire/present logic.
  - `VulkanTextureStore` (`src/impl/ecs/graphics/`) — texture CRUD (`createTexture`, `uploadTexture`, `destroyTexture`), staging upload with fence-guarded cleanup, and entity texture descriptor set management.
  - `VulkanPipelineStore` (`src/impl/ecs/graphics/`) — pipeline and shader module CRUD; maps opaque `PipelineHandle`/`ShaderModuleHandle` to internal `vk::Pipeline`/`vk::ShaderModule` objects.
  - `VulkanFrameRenderer` (`src/impl/ecs/graphics/`) — per-frame command recording; owns SDF pipeline, per-image glyph/instance buffers, entity draw group dispatch, and rotation animation state.
- `VulkanTypes` (`src/impl/ecs/graphics/vulkantypes.h`) — shared `PushConstants` struct used by swapchain pipeline layout and frame renderer push constant writes.
- `IGraphicsBackend::createTexture()` and `uploadTexture()` manage texture creation and pixel upload; `VulkanAPI` delegates these to `VulkanTextureStore` which uses staging buffers and image layout transitions.
- SDF atlas for text rendering is implemented via `TextureHandle` and managed through `IGraphicsBackend` methods instead of direct Vulkan objects.
- Entity pipelines are created on demand by `PipelineCache`, not hardcoded in `createSwapchain()`.
- `PostgreSQLSystem : AbstractSystem`
- `PostgreSQLResult : Result`
- `TextData : Data`
- `TextComponent` (render extension for voxelized text instances via FreeType)
- `TextEditState` (render extension for in-world text editing; manages UTF-8 text buffer, cursor position, and blink timer)
- `TextEditorSystem` (example/window subsystem that owns editor interaction logic: cursor placement by ray hit, line wrapping, enter/newline behavior, and render text refresh)
- `SetupTextEditTask` generates bitmap-style text where each character is a small colored plane
- `MeshComponent::createPlane()` creates flat rectangular meshes for UI elements
- `Vertex` now supports optional UV coordinates (texCoord) for future bitmap font texture mapping

### Supporting contracts

- `ISignalPayload` is the base payload interface carried by signal messages; each payload declares its `PayloadTypeId`.
- `ISignalEmitter` is the facade that posts payloads onto the underlying `IMessageBus` and exposes `subscribeSignal` for handlers.
- `ITaskFlow` connects two tasks by subscribing the receiver (as `ISubscriber`) for a payload id, with an optional thread-affinity for the delivery hop.
- `IWindowEventHandlerComponent` defines the window input and lifecycle callback interface.
- `AbstractComponent` and `AbstractEntity` are base ECS abstractions kept for component/entity specializations.
- Window Vulkan example init flow includes `SetupTextTask` and `SetupTextEditTask` to render voxelized text and set up an in-world text editor.
- `TextEditState` (example-level) manages UTF-8 text buffer, cursor position, and blink timing for the text editor.
- `RunWindowTask` can toggle mouse-ray visualization (`R`) without affecting ray-based editor behavior and picking logic.
- `MeshComponent::createPlane(width, height, color)` creates a flat rectangular plane mesh for use as UI panels or backgrounds.

### Documentation maintenance

- Keep this file aligned with the current engine structure when folders, core classes, or inheritance relations change.
- If class relationships change, also update the related Mermaid diagrams in this folder.

## Architecture rules

- [Engine token (R-StateScope pattern)](ecs/engine-token.md) —
  state-scoped DI handle the engine hands to tasks via
  `IContext::makeEngineToken`. Documents the
  `IEngineToken` / `AbstractEngineToken` / `EngineToken` pyramid, the
  hybrid gating policy that splits domain accessors (gated, return
  `Result<T>`) from infrastructure accessors (non-gated, return `T&`),
  the `subscribeExpiration` self-destruct contract, and the FSM-side
  invalidation listener firing path on `AbstractStateMachine`.
- [Task system reference](ecs/system.md) — task-side companion to
  `engine-token.md`. Documents the `ITask::setApi` / `ITask::api()`
  surface, the gated vs non-gated accessor split as seen from inside a
  task body, and the `ApiBindingGuard` RAII pattern the task flow
  drives around every `run()` call.

## Class diagrams

- [Core architecture](core-class-diagram.md)
- [Example application classes](example-class-diagram.md)
- [Window signal subsystem](signals-class-diagram.md) (includes sequence flow)

## Sequence diagrams

- [Engine run and state transitions](sequence-engine-state.md)
- [Window input signal dispatch](sequence-window-signal.md)
- [All task flows grouped by folders](sequence-all-tasks.md)
- Folder [doc/sequence/postgresql](sequence/postgresql/README.md)
- Folder [doc/sequence/window](sequence/window/README.md)

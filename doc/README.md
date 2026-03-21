# Vigine Project Diagrams

This folder contains project structure visualizations in Mermaid format.

## Engine structure

If a task description does not include the project folder layout, use this section as the quick map of the engine.

### Main folders

- `include/vigine/` public engine API and core abstractions.
- `include/vigine/base/` basic shared types and helpers.
- `include/vigine/service/` public service interfaces and implementations exposed through `Context`.
- `include/vigine/signal/` signal contracts used by `TaskFlow`.
- `include/vigine/ecs/` ECS-facing engine types.
- `include/vigine/ecs/platform/` platform-agnostic window and input interfaces.
- `include/vigine/ecs/render/` rendering interfaces and systems.
- `include/vigine/ecs/postgresql/` PostgreSQL data and system API.
- `src/` engine implementations for the public headers.
- `src/ecs/platform/` current Windows-specific window components and dispatch internals.
- `src/ecs/render/` rendering implementation.
- `src/ecs/postgresql/` PostgreSQL implementation details.
- `example/` example applications built on top of the engine.
- `test/` tests for engine components and architecture.

### Core engine classes

- `Engine` owns `StateMachine`, `Context`, and `EntityManager`.
- `Context` creates and stores service and system instances and gives access to `EntityManager`.
- `StateMachine` stores `AbstractState` instances and transitions between them by `Result::Code`.
- `AbstractState` owns one `TaskFlow` and drives `enter()` / `exit()` lifecycle hooks.
- `TaskFlow` stores `AbstractTask` instances, routes them by `Result::Code`, and can connect tasks through `ISignalBinder`.
- `Result` is the shared execution result type for states, tasks, and transitions.
- `EntityManager` owns engine entities.
- `Entity` is the base runtime entity type.
- `EntityBindingHost` provides entity binding hooks for services and systems.
- `ContextHolder` provides `Context` propagation for tasks and services.

### Inheritance overview

- `AbstractTask : ContextHolder`
- `AbstractService : ContextHolder, EntityBindingHost`
- `AbstractSystem : EntityBindingHost`
- `PlatformService : AbstractService`
- `GraphicsService : AbstractService`
- `DatabaseService : AbstractService`
- `WindowSystem : AbstractSystem`
- `RenderSystem : AbstractSystem`
- `PostgreSQLSystem : AbstractSystem`
- `PostgreSQLResult : Result`
- `TextData : Data`
- `WinAPIComponent : WindowComponent`
- `WindowEventDispatcher : IWindowEventHandlerComponent`

### Supporting contracts

- `ISignal` is the base signal interface.
- `ISignalEmiter` is implemented by tasks that emit signals.
- `ISignalBinder` validates whether two tasks may be connected by a signal route.
- `IWindowEventHandlerComponent` defines the window input and lifecycle callback interface.
- `AbstractComponent` and `AbstractEntity` are base ECS abstractions kept for component/entity specializations.

### Documentation maintenance

- Keep this file aligned with the current engine structure when folders, core classes, or inheritance relations change.
- If class relationships change, also update the related Mermaid diagrams in this folder.

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

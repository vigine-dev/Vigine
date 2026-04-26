# Sequence Diagram: Engine Run and State Transitions

```mermaid
sequenceDiagram
participant Main as example/*/main.cpp
participant Engine as vigine::engine::IEngine
participant Ctx as IContext
participant FSM as IStateMachine
participant TF as ITaskFlow
participant Task as ITask

Main->>Engine: run()
loop per pump tick
  Engine->>Ctx: stateMachine() / taskFlow()
  Engine->>FSM: drain pending transitions
  Engine->>TF: hasTasksToRun()
  alt has runnable
    Engine->>Ctx: makeEngineToken(boundState)
    Engine->>Task: setApi(token)
    Engine->>Task: run()
    Task-->>Engine: Result
    Engine->>Task: setApi(nullptr)
    Engine->>TF: advance cursor by Result::Code
  end
  Engine->>FSM: select next state by transition outcome
end
Engine-->>Main: returns on shutdown
```

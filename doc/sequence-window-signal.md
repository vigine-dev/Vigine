# Sequence Diagram: Window Input Signal Dispatch

```mermaid
sequenceDiagram
participant Main as main.cpp
participant TF as TaskFlow
participant Emitter as ISignalEmitter
participant Bus as IMessageBus
participant Run as RunWindowTask
participant Handler as WindowEventHandler
participant Disp as WindowEventDispatcher
participant Proc as ProcessInputEventTask
participant Render as RenderSystem/VulkanAPI

Main->>TF: setSignalEmitter(emitter)
Main->>TF: signal(Run, Proc, idOf(MouseButtonDownPayload::typeName()))
TF->>Emitter: subscribeSignal({Signal, idOf(MouseButtonDownPayload::typeName())}, Proc)
Emitter->>Bus: subscribe(filter, Proc)
Bus-->>Emitter: ISubscriptionToken
Emitter-->>TF: ISubscriptionToken

Main->>TF: signal(Run, Proc, idOf(KeyDownPayload::typeName()))
TF->>Emitter: subscribeSignal({Signal, idOf(KeyDownPayload::typeName())}, Proc)
Emitter->>Bus: subscribe(filter, Proc)
Bus-->>Emitter: ISubscriptionToken
Emitter-->>TF: ISubscriptionToken

Note over TF,Emitter: TaskFlow::signal stores the tokens and keeps ProcessInputEventTask subscribed until the flow is destroyed.

Run->>Handler: setMouseButtonDownCallback(lambda)
Run->>Handler: setKeyDownCallback(lambda)
Run->>Handler: setKeyUpCallback(lambda)

Disp->>Handler: onMouseButtonDown(button,x,y)
Handler-->>Run: callback(button,x,y)
Run->>Render: beginCameraDrag(x,y) for left button
Run->>Emitter: emit(make_unique<MouseButtonDownPayload>(...))
Emitter->>Bus: post(SignalMessage)
Bus->>Proc: onMessage(SignalMessage)

Disp->>Handler: onMouseMove(x,y)
Handler-->>Run: callback(x,y)
Run->>Render: updateCameraDrag(x,y)

Disp->>Handler: onMouseWheel(delta,x,y)
Handler-->>Run: callback(delta,x,y)
Run->>Render: zoomCamera(delta)

Disp->>Handler: onMouseButtonUp(button,x,y)
Handler-->>Run: callback(button,x,y)
Run->>Render: endCameraDrag() for left button

Disp->>Handler: onKeyDown(event)
Handler-->>Run: callback(event)
Run->>Render: setMoveForward/Back/Left/Right(true) for WASD
Run->>Render: setMoveUp/Down(true) for E/Q, setSprint(true) for Shift
Run->>Emitter: emit(make_unique<KeyDownPayload>(...))
Emitter->>Bus: post(SignalMessage)
Bus->>Proc: onMessage(SignalMessage)

Disp->>Handler: onKeyUp(event)
Handler-->>Run: callback(event)
Run->>Render: setMoveForward/Back/Left/Right(false) for WASD
Run->>Render: setMoveUp/Down(false) for E/Q, setSprint(false) for Shift
```

## Keyboard And Mouse Controls

- `W` move camera forward.
- `S` move camera backward.
- `A` strafe camera left.
- `D` strafe camera right.
- `E` move camera up.
- `Q` move camera down.
- `Shift` hold to enable sprint multiplier for movement speed.
- `Left Mouse Button + Move` mouse-look (camera yaw/pitch).
- `Mouse Wheel` move camera forward/backward along current view direction.

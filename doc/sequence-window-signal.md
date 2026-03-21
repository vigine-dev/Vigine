# Sequence Diagram: Window Input Signal Dispatch

```mermaid
sequenceDiagram
participant Main as createInitTaskFlow
participant TF as TaskFlow
participant MB as MouseEventSignalBinder
participant KB as KeyEventSignalBinder
participant Run as RunWindowTask
participant Handler as WindowEventHandler
participant Disp as WindowEventDispatcher
participant Proxy as ISignalEmiter::SignalEmiterProxy
participant Proc as ProcessInputEventTask
participant Render as RenderSystem/VulkanAPI

Main->>TF: signal(Run, Proc, MB)
TF->>MB: check(Run, Proc)
MB->>Run: setProxyEmiter(lambda)
MB-->>TF: true

Main->>TF: signal(Run, Proc, KB)
TF->>KB: check(Run, Proc)
KB->>Run: setProxyEmiter(lambda)
KB-->>TF: true

Note over TF,KB: TaskFlow::signal() currently validates binder compatibility and installs emitter proxies.

Run->>Handler: setMouseButtonDownCallback(lambda)
Run->>Handler: setKeyDownCallback(lambda)
Run->>Handler: setKeyUpCallback(lambda)

Disp->>Handler: onMouseButtonDown(button,x,y)
Handler-->>Run: callback(button,x,y)
Run->>Run: emitMouseButtonDownSignal(...)
Run->>Render: beginCameraDrag(x,y) for left button
Run->>Proxy: proxyEmiter()(new MouseButtonDownSignal)
Proxy->>Proc: onMouseButtonDownSignal(signal)

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
Run->>Run: emitKeyDownSignal(event)
Run->>Proxy: proxyEmiter()(new KeyDownSignal)
Proxy->>Proc: onKeyDownSignal(signal)

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

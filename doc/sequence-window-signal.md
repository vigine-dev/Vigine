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

Disp->>Handler: onMouseButtonDown(button,x,y)
Handler-->>Run: callback(button,x,y)
Run->>Run: emitMouseButtonDownSignal(...)
Run->>Proxy: proxyEmiter()(new MouseButtonDownSignal)
Proxy->>Proc: onMouseButtonDownSignal(signal)

Disp->>Handler: onKeyDown(event)
Handler-->>Run: callback(event)
Run->>Run: emitKeyDownSignal(event)
Run->>Proxy: proxyEmiter()(new KeyDownSignal)
Proxy->>Proc: onKeyDownSignal(signal)
```

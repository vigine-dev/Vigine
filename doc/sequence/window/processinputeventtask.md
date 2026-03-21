# ProcessInputEventTask

```mermaid
sequenceDiagram
autonumber
participant Caller as TaskFlow
participant T as ProcessInputEventTask
participant Run as RunWindowTask
participant Proxy as Binder proxy
participant M as MouseButtonDownSignal
participant K as KeyDownSignal

Caller->>T: execute()
T-->>Caller: Result(Success)

rect rgb(245, 248, 255)
Note over T,M: Signal callback path for mouse
Run->>Proxy: proxyEmiter()(new MouseButtonDownSignal)
Proxy-->>T: onMouseButtonDownSignal(M*)
alt signal is null
  T-->>Proxy: return
else signal exists
  T->>T: set _hasMouseEvent = true
  T->>T: print mouse event
end
end

rect rgb(245, 255, 245)
Note over T,K: Signal callback path for keyboard
Run->>Proxy: proxyEmiter()(new KeyDownSignal)
Proxy-->>T: onKeyDownSignal(K*)
alt signal is null
  T-->>Proxy: return
else signal exists
  T->>T: set _hasKeyEvent = true
  T->>T: print key event
end
end
```

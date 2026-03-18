# AddSomeDataTask

```mermaid
sequenceDiagram
autonumber
participant Caller as TaskFlow
participant T as AddSomeDataTask
participant Ctx as Context
participant EM as EntityManager
participant DB as DatabaseService

Caller->>T: execute()
T->>T: print start message
T->>Ctx: entityManager()
Ctx-->>T: EntityManager*
T->>EM: getEntityByAlias(PostgresBDLocal)
EM-->>T: entity

T->>DB: bindEntity(entity)
Note over T,DB: Data insert loop is commented in current code
T->>DB: unbindEntity()
T-->>Caller: Result(Success)
```

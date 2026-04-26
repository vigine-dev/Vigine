// ---------------------------------------------------------------------------
// example-postgresql
//
// Drives the experimental Postgres demo through the modern engine
// (`vigine::engine::createEngine`) and the per-state TaskFlow registry
// added in #334. Every state owns one TaskFlow registered through
// `IStateMachine::addStateTaskFlow`; the engine looks the bound flow up
// each tick and pumps `runCurrentTask` until the flow signals
// completion. State transitions are driven by tasks that call
// `IStateMachine::requestTransition` on the FSM the engine owns.
//
// What the demo proves
// --------------------
//   * Modern engine wiring: `vigine::engine::createEngine` + the
//     IStateMachine + TaskFlow registry are enough to reproduce the
//     legacy Init -> Work -> Close (with Error fallback) loop.
//   * Modern service container: `DatabaseService` (post #330) is
//     registered through `IContext::registerService` and the underlying
//     `PostgreSQLSystem` is attached via `setPostgresSystem` so the
//     CRUD entry points have a live driver to delegate to.
//   * Experimental gate: every database-facing call is compiled in only
//     when `VIGINE_POSTGRESQL` is defined. The default build
//     (`VIGINE_ENABLE_EXPERIMENTAL=OFF`) skips the demo entirely; this
//     translation unit therefore compiles in a configuration that has
//     `VIGINE_POSTGRESQL` set as well.
// ---------------------------------------------------------------------------

#include "task/data/addsomedatatask.h"
#include "task/data/readsomedatatask.h"
#include "task/data/removesomedatatask.h"
#include "task/db/checkbdshecmetask.h"
#include "task/db/initbdtask.h"
#include "task/transitiontask.h"

#include <vigine/api/context/icontext.h>
#include <vigine/api/engine/factory.h>
#include <vigine/api/engine/iengine.h>
#include <vigine/api/statemachine/istatemachine.h>
#include <vigine/api/statemachine/stateid.h>
#include <vigine/api/taskflow/factory.h>
#include <vigine/api/taskflow/itaskflow.h>
#include <vigine/api/taskflow/resultcode.h>
#include <vigine/api/taskflow/taskid.h>
#include <vigine/result.h>
#include <vigine/impl/service/databaseservice.h>

#if VIGINE_POSTGRESQL
#include <vigine/experimental/ecs/postgresql/impl/postgresqlsystem.h>
#endif

#include <iostream>
#include <memory>
#include <utility>

namespace
{

// All per-state task flows are built on the same shape: a linear chain
// of domain tasks, ending in one or more transition tasks that hand
// control back to the FSM through `requestTransition`. The success
// route always advances to the next phase; the error route diverts to
// the error state (or, in the close phase, asks the engine to shut
// down).

std::unique_ptr<vigine::taskflow::ITaskFlow> buildInitFlow(
    vigine::DatabaseService             *dbService,
    vigine::statemachine::IStateMachine *stateMachine,
    vigine::statemachine::StateId        workState,
    vigine::statemachine::StateId        errorState)
{
    using vigine::taskflow::ResultCode;
    using vigine::taskflow::TaskId;

    auto flow = vigine::taskflow::createTaskFlow();

    auto initBd        = std::make_unique<InitBDTask>();
    initBd->setDatabaseService(dbService);
    auto checkSchema   = std::make_unique<CheckBDShecmeTask>();
    checkSchema->setDatabaseService(dbService);
    auto toWork        = std::make_unique<TransitionTask>(stateMachine, workState);
    auto toError       = std::make_unique<TransitionTask>(stateMachine, errorState);

    const TaskId initBdId      = flow->addTask();
    const TaskId checkSchemaId = flow->addTask();
    const TaskId toWorkId      = flow->addTask();
    const TaskId toErrorId     = flow->addTask();

    static_cast<void>(flow->attachTaskRun(initBdId, std::move(initBd)));
    static_cast<void>(flow->attachTaskRun(checkSchemaId, std::move(checkSchema)));
    static_cast<void>(flow->attachTaskRun(toWorkId, std::move(toWork)));
    static_cast<void>(flow->attachTaskRun(toErrorId, std::move(toError)));

    static_cast<void>(flow->onResult(initBdId,      ResultCode::Success, checkSchemaId));
    static_cast<void>(flow->onResult(initBdId,      ResultCode::Error,   toErrorId));
    static_cast<void>(flow->onResult(checkSchemaId, ResultCode::Success, toWorkId));
    static_cast<void>(flow->onResult(checkSchemaId, ResultCode::Error,   toErrorId));

    static_cast<void>(flow->setRoot(initBdId));
    return flow;
}

std::unique_ptr<vigine::taskflow::ITaskFlow> buildWorkFlow(
    vigine::DatabaseService             *dbService,
    vigine::statemachine::IStateMachine *stateMachine,
    vigine::statemachine::StateId        closeState,
    vigine::statemachine::StateId        errorState)
{
    using vigine::taskflow::ResultCode;
    using vigine::taskflow::TaskId;

    auto flow = vigine::taskflow::createTaskFlow();

    auto addData    = std::make_unique<AddSomeDataTask>();
    addData->setDatabaseService(dbService);
    auto readData   = std::make_unique<ReadSomeDataTask>();
    readData->setDatabaseService(dbService);
    auto removeData = std::make_unique<RemoveSomeDataTask>();
    removeData->setDatabaseService(dbService);
    auto toClose    = std::make_unique<TransitionTask>(stateMachine, closeState);
    auto toError    = std::make_unique<TransitionTask>(stateMachine, errorState);

    const TaskId addId    = flow->addTask();
    const TaskId readId   = flow->addTask();
    const TaskId removeId = flow->addTask();
    const TaskId closeId  = flow->addTask();
    const TaskId errorId  = flow->addTask();

    static_cast<void>(flow->attachTaskRun(addId,    std::move(addData)));
    static_cast<void>(flow->attachTaskRun(readId,   std::move(readData)));
    static_cast<void>(flow->attachTaskRun(removeId, std::move(removeData)));
    static_cast<void>(flow->attachTaskRun(closeId,  std::move(toClose)));
    static_cast<void>(flow->attachTaskRun(errorId,  std::move(toError)));

    static_cast<void>(flow->onResult(addId,    ResultCode::Success, readId));
    static_cast<void>(flow->onResult(addId,    ResultCode::Error,   errorId));
    static_cast<void>(flow->onResult(readId,   ResultCode::Success, removeId));
    static_cast<void>(flow->onResult(readId,   ResultCode::Error,   errorId));
    static_cast<void>(flow->onResult(removeId, ResultCode::Success, closeId));
    static_cast<void>(flow->onResult(removeId, ResultCode::Error,   errorId));

    static_cast<void>(flow->setRoot(addId));
    return flow;
}

std::unique_ptr<vigine::taskflow::ITaskFlow> buildErrorFlow(
    vigine::statemachine::IStateMachine *stateMachine,
    vigine::statemachine::StateId        closeState)
{
    auto flow = vigine::taskflow::createTaskFlow();

    // Error phase prints the reached-error notice and falls through to
    // close so the engine can shut down cleanly. No domain work runs
    // in this state.
    auto toClose = std::make_unique<TransitionTask>(stateMachine, closeState);
    const vigine::taskflow::TaskId toCloseId = flow->addTask();
    static_cast<void>(flow->attachTaskRun(toCloseId, std::move(toClose)));
    static_cast<void>(flow->setRoot(toCloseId));
    return flow;
}

std::unique_ptr<vigine::taskflow::ITaskFlow> buildCloseFlow(vigine::engine::IEngine *engine)
{
    auto flow = vigine::taskflow::createTaskFlow();

    // Close phase asks the engine to stop the main loop. The
    // ShutdownTask returns Success, the flow has no further routes
    // out of it, and Engine::run() exits on the next pump tick.
    auto shutdown = std::make_unique<ShutdownTask>(engine);
    const vigine::taskflow::TaskId shutdownId = flow->addTask();
    static_cast<void>(flow->attachTaskRun(shutdownId, std::move(shutdown)));
    static_cast<void>(flow->setRoot(shutdownId));
    return flow;
}

} // namespace

int main()
{
    // ---- Engine ----------------------------------------------------------
    auto engine = vigine::engine::createEngine();
    if (!engine)
    {
        std::cerr << "createEngine returned null\n";
        return 1;
    }

    auto &context      = engine->context();
    auto &stateMachine = context.stateMachine();

    // ---- Database service -----------------------------------------------
    //
    // The service is constructed with its instance name (the legacy
    // registry used the Name to distinguish multiple service instances;
    // the modern container stamps a generational ServiceId on
    // registerService). We keep the shared_ptr around because we still
    // need a non-owning DatabaseService* to hand into each task and to
    // attach the postgres system after registration.
    auto dbService = std::make_shared<vigine::DatabaseService>(vigine::Name("TestDB"));

    if (const auto regResult = context.registerService(dbService);
        regResult.isError())
    {
        std::cerr << "registerService failed: " << regResult.message() << '\n';
        return 1;
    }

#if VIGINE_POSTGRESQL
    // The PostgreSQLSystem is the substrate the service delegates to.
    // It is built standalone and attached through `setPostgresSystem`;
    // entity binding is performed locally because the modern IECS
    // surface does not yet expose a system-locator and the legacy
    // entity-bind path is not part of the modern token contract.
    auto pgSystem = std::make_unique<vigine::experimental::ecs::postgresql::PostgreSQLSystem>(
        vigine::SystemName{"postgres-system"});
    dbService->setPostgresSystem(pgSystem.get());
#endif

    // ---- States + per-state TaskFlows -----------------------------------
    //
    // The auto-provisioned default state from AbstractStateMachine is
    // left unused; we register explicit Init / Work / Error / Close
    // states and pin the FSM to Init via `setInitial`. The TaskFlows
    // are wired with explicit success / error routes so the legacy
    // result-code-keyed transitions (previously encoded on the legacy
    // StateMachine through `addTransition`) survive in the modern
    // shape: each task's Result drives the in-flow advance, and the
    // terminal TransitionTask asks the FSM to switch to the next
    // state once the chain reaches its end.
    const auto initStateId  = stateMachine.addState();
    const auto workStateId  = stateMachine.addState();
    const auto errorStateId = stateMachine.addState();
    const auto closeStateId = stateMachine.addState();

    if (!initStateId.valid() || !workStateId.valid() ||
        !errorStateId.valid() || !closeStateId.valid())
    {
        std::cerr << "addState returned an invalid id\n";
        return 1;
    }

    if (const auto initResult = stateMachine.setInitial(initStateId);
        initResult.isError())
    {
        std::cerr << "setInitial failed: " << initResult.message() << '\n';
        return 1;
    }

    {
        auto flow = buildInitFlow(dbService.get(), &stateMachine,
                                  workStateId, errorStateId);
        if (const auto bind = stateMachine.addStateTaskFlow(initStateId, std::move(flow));
            bind.isError())
        {
            std::cerr << "addStateTaskFlow(init) failed: " << bind.message() << '\n';
            return 1;
        }
    }

    {
        auto flow = buildWorkFlow(dbService.get(), &stateMachine,
                                  closeStateId, errorStateId);
        if (const auto bind = stateMachine.addStateTaskFlow(workStateId, std::move(flow));
            bind.isError())
        {
            std::cerr << "addStateTaskFlow(work) failed: " << bind.message() << '\n';
            return 1;
        }
    }

    {
        auto flow = buildErrorFlow(&stateMachine, closeStateId);
        if (const auto bind = stateMachine.addStateTaskFlow(errorStateId, std::move(flow));
            bind.isError())
        {
            std::cerr << "addStateTaskFlow(error) failed: " << bind.message() << '\n';
            return 1;
        }
    }

    {
        auto flow = buildCloseFlow(engine.get());
        if (const auto bind = stateMachine.addStateTaskFlow(closeStateId, std::move(flow));
            bind.isError())
        {
            std::cerr << "addStateTaskFlow(close) failed: " << bind.message() << '\n';
            return 1;
        }
    }

    // ---- Run -------------------------------------------------------------
    const auto runResult = engine->run();

#if VIGINE_POSTGRESQL
    // Detach the postgres system from the service before the system
    // goes out of scope. Without this the service would observe a
    // dangling pointer on any post-shutdown CRUD call.
    dbService->setPostgresSystem(nullptr);
#endif

    return runResult.isSuccess() ? 0 : 1;
}

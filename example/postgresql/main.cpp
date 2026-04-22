#include <vigine/statemachine.h>
#include <vigine/taskflow.h>
#include <vigine/vigine.h>

#include "state/closestate.h"
#include "state/errorstate.h"
#include "state/initstate.h"
#include "state/workstate.h"
#include "task/data/addsomedatatask.h"
#include "task/data/readsomedatatask.h"
#include "task/data/removesomedatatask.h"
#include "task/db/checkbdshecmetask.h"
#include "task/db/initbdtask.h"

#include <cassert>
#include <memory>


using namespace vigine;

std::unique_ptr<TaskFlow> createInitTaskFlow()
{
    auto taskFlow           = std::make_unique<TaskFlow>();

    auto *initDBTask        = taskFlow->addTask(std::make_unique<InitBDTask>());
    auto *checkBDShecmeTask = taskFlow->addTask(std::make_unique<CheckBDShecmeTask>());

    static_cast<void>(taskFlow->route(initDBTask, checkBDShecmeTask));
    taskFlow->changeCurrentTaskTo(initDBTask);

    return taskFlow;
}

std::unique_ptr<TaskFlow> createWorkTaskFlow()
{
    auto taskFlow            = std::make_unique<TaskFlow>();

    auto *addSomeDataTask    = taskFlow->addTask(std::make_unique<AddSomeDataTask>());
    auto *readSomeDataTask   = taskFlow->addTask(std::make_unique<ReadSomeDataTask>());
    auto *removeSomeDataTask = taskFlow->addTask(std::make_unique<RemoveSomeDataTask>());

    static_cast<void>(taskFlow->route(addSomeDataTask, readSomeDataTask));
    static_cast<void>(taskFlow->route(readSomeDataTask, removeSomeDataTask));
    taskFlow->changeCurrentTaskTo(addSomeDataTask);

    return taskFlow;
}

std::unique_ptr<TaskFlow> createErrorTaskFlow() { return std::make_unique<TaskFlow>(); }

std::unique_ptr<TaskFlow> createCloseTaskFlow() { return std::make_unique<TaskFlow>(); }

int main()
{
    Engine engine;
    // Engine::state() now returns IStateMachine&; the concrete
    // StateMachine is still the only implementation shipped with the
    // engine, so downcast back to it for the rich state-machine API.
    // Later leaves move the addState/addTransition surface onto the
    // interface itself; the cast is temporary scaffolding.
    StateMachine *stMachine = dynamic_cast<StateMachine *>(&engine.state());
    assert(stMachine != nullptr &&
           "Engine::state() must be a concrete StateMachine until the "
           "rich API lifts onto IStateMachine");

    auto initState          = std::make_unique<InitState>();
    auto workState          = std::make_unique<WorkState>();
    auto errorState         = std::make_unique<ErrorState>();
    auto closeState         = std::make_unique<CloseState>();

    auto initPtr            = stMachine->addState(std::move(initState));
    auto workPtr            = stMachine->addState(std::move(workState));
    auto errorPtr           = stMachine->addState(std::move(errorState));
    auto closePtr           = stMachine->addState(std::move(closeState));

    initPtr->setTaskFlow(createInitTaskFlow());
    workPtr->setTaskFlow(createWorkTaskFlow());
    errorPtr->setTaskFlow(createErrorTaskFlow());
    closePtr->setTaskFlow(createCloseTaskFlow());

    static_cast<void>(stMachine->addTransition(initPtr, workPtr, Result::Code::Success));
    static_cast<void>(stMachine->addTransition(initPtr, errorPtr, Result::Code::Error));
    static_cast<void>(stMachine->addTransition(workPtr, closePtr, Result::Code::Success));
    static_cast<void>(stMachine->addTransition(workPtr, errorPtr, Result::Code::Error));
    static_cast<void>(stMachine->addTransition(errorPtr, workPtr, Result::Code::Success));
    static_cast<void>(stMachine->addTransition(errorPtr, closePtr, Result::Code::Error));

    stMachine->changeStateTo(initPtr);
    engine.run();

    return 0;
}

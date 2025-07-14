#include <vigine/statemachine.h>
#include <vigine/taskflow.h>
#include <vigine/vigine.h>

#include "state/closestate.h"
#include "state/errorstate.h"
#include "state/initstate.h"
#include "state/workstate.h"
#include "task/addsomedatatask.h"
#include "task/checkbdshecmetask.h"
#include "task/initbdtask.h"
#include "task/readsomedatatask.h"
#include "task/removesomedatatask.h"

#include <glm/glm.hpp>
#include <iostream>
#include <memory>

using namespace vigine;

//  * find best way to add context
//    * Check if the context is adding correctly

// create service Database
// create system for work with DB
// init DB
// make sql query

std::unique_ptr<TaskFlow> createInitTaskFlow()
{
    auto taskFlow           = std::make_unique<TaskFlow>();
    auto *initDBTask        = taskFlow->addTask(std::make_unique<InitBDTask>());
    auto *checkBDShecmeTask = taskFlow->addTask(std::make_unique<CheckBDShecmeTask>());

    taskFlow->addTransition(initDBTask, checkBDShecmeTask, Result::Code::Success);
    taskFlow->changeCurrentTaskTo(initDBTask);

    return std::move(taskFlow);
}

std::unique_ptr<TaskFlow> createWorkTaskFlow()
{
    auto taskFlow          = std::make_unique<TaskFlow>();
    auto *addSomeDataTask  = taskFlow->addTask(std::make_unique<AddSomeDataTask>());
    auto *readSomeDataTask = taskFlow->addTask(std::make_unique<ReadSomeDataTask>());
    auto *removeSomeDataTask = taskFlow->addTask(std::make_unique<RemoveSomeDataTask>());

    // taskFlow->addTransition(addSomeDataTask, readSomeDataTask, Result::Code::Success);
    // taskFlow->addTransition(readSomeDataTask, removeSomeDataTask, Result::Code::Success);
    // taskFlow->changeCurrentTaskTo(addSomeDataTask);

    return std::move(taskFlow);
}

std::unique_ptr<TaskFlow> createErrorTaskFlow()
{
    auto taskFlow = std::make_unique<TaskFlow>();

    return std::move(taskFlow);
}

std::unique_ptr<TaskFlow> createCloseTaskFlow()
{
    auto taskFlow = std::make_unique<TaskFlow>();

    return std::move(taskFlow);
}

int main()
{
    Engine engine;
    StateMachine *stMachine = engine.state();

    // init states
    auto initState  = std::make_unique<InitState>();
    auto workState  = std::make_unique<WorkState>();
    auto errorState = std::make_unique<ErrorState>();
    auto closeState = std::make_unique<CloseState>();

    // Add states to Vigine
    auto initPtr       = stMachine->addState(std::move(initState));
    auto workPtr       = stMachine->addState(std::move(workState));
    auto errorPtr      = stMachine->addState(std::move(errorState));
    auto closePtr      = stMachine->addState(std::move(closeState));

    auto initTaskFlow  = createInitTaskFlow();
    auto workTaskFlow  = createWorkTaskFlow();
    auto errorTaskFlow = createErrorTaskFlow();
    auto closeTaskFlow = createCloseTaskFlow();

    initPtr->setTaskFlow(std::move(initTaskFlow));
    workPtr->setTaskFlow(std::move(workTaskFlow));
    errorPtr->setTaskFlow(std::move(errorTaskFlow));
    closePtr->setTaskFlow(std::move(closeTaskFlow));

    // Add transitions
    stMachine->addTransition(initPtr, workPtr, Result::Code::Success);
    stMachine->addTransition(initPtr, errorPtr, Result::Code::Error);

    stMachine->addTransition(workPtr, closePtr, Result::Code::Success);
    stMachine->addTransition(workPtr, errorPtr, Result::Code::Error);

    stMachine->addTransition(errorPtr, workPtr, Result::Code::Success);
    stMachine->addTransition(errorPtr, closePtr, Result::Code::Error);

    stMachine->changeStateTo(initPtr);

    engine.run();

    return 0;
}

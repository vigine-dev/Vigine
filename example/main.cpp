#include "vigine/base/function.h"
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
#include "task/window/initwindowtask.h"
#include "task/window/processinputeventtask.h"
#include "task/window/runwindowtask.h"
#include "task/window/windoweventsignal.h"

#include <glm/glm.hpp>
#include <iostream>
#include <memory>
#include <string>

using namespace vigine;

std::unique_ptr<TaskFlow> createInitTaskFlow(MouseEventSignalBinder &mouseSignalBinder,
                                             KeyEventSignalBinder &keySignalBinder)
{
    auto taskFlow               = std::make_unique<TaskFlow>();
    auto *initDBTask            = taskFlow->addTask(std::make_unique<InitBDTask>());
    auto *checkBDShecmeTask     = taskFlow->addTask(std::make_unique<CheckBDShecmeTask>());
    auto *initWindow            = taskFlow->addTask(std::make_unique<InitWindowTask>());
    auto *runWindow             = taskFlow->addTask(std::make_unique<RunWindowTask>());
    auto *processInputEventTask = taskFlow->addTask(std::make_unique<ProcessInputEventTask>());

    static_cast<void>(taskFlow->route(initDBTask, checkBDShecmeTask));
    static_cast<void>(taskFlow->route(checkBDShecmeTask, initWindow));
    static_cast<void>(taskFlow->route(initWindow, runWindow));

    static_cast<void>(taskFlow->signal(runWindow, processInputEventTask, &mouseSignalBinder));
    static_cast<void>(taskFlow->signal(runWindow, processInputEventTask, &keySignalBinder));

    taskFlow->changeCurrentTaskTo(initDBTask);

    return taskFlow;
}

std::unique_ptr<TaskFlow> createWorkTaskFlow()
{
    auto taskFlow            = std::make_unique<TaskFlow>();
    auto *addSomeDataTask    = taskFlow->addTask(std::make_unique<AddSomeDataTask>());
    auto *readSomeDataTask   = taskFlow->addTask(std::make_unique<ReadSomeDataTask>());
    auto *removeSomeDataTask = taskFlow->addTask(std::make_unique<RemoveSomeDataTask>());

    // taskFlow->addTransition(addSomeDataTask, readSomeDataTask, Result::Code::Success);
    // taskFlow->addTransition(readSomeDataTask, removeSomeDataTask, Result::Code::Success);
    // taskFlow->changeCurrentTaskTo(addSomeDataTask);

    return taskFlow;
}

std::unique_ptr<TaskFlow> createErrorTaskFlow()
{
    auto taskFlow = std::make_unique<TaskFlow>();

    return taskFlow;
}

std::unique_ptr<TaskFlow> createCloseTaskFlow()
{
    auto taskFlow = std::make_unique<TaskFlow>();

    return taskFlow;
}

int main()
{
    Engine engine;
    StateMachine *stMachine = engine.state();
    MouseEventSignalBinder mouseSignalBinder;
    KeyEventSignalBinder keySignalBinder;

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

    auto initTaskFlow  = createInitTaskFlow(mouseSignalBinder, keySignalBinder);
    auto workTaskFlow  = createWorkTaskFlow();
    auto errorTaskFlow = createErrorTaskFlow();
    auto closeTaskFlow = createCloseTaskFlow();

    initPtr->setTaskFlow(std::move(initTaskFlow));
    workPtr->setTaskFlow(std::move(workTaskFlow));
    errorPtr->setTaskFlow(std::move(errorTaskFlow));
    closePtr->setTaskFlow(std::move(closeTaskFlow));

    // Add transitions
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

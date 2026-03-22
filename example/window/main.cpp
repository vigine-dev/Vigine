#include "texteditstate.h"

#include <vigine/statemachine.h>
#include <vigine/taskflow.h>
#include <vigine/vigine.h>

#include "state/closestate.h"
#include "state/errorstate.h"
#include "state/initstate.h"
#include "state/workstate.h"
#include "system/texteditorsystem.h"
#include "task/vulkan/initvulkantask.h"
#include "task/vulkan/rendercubetask.h"
#include "task/vulkan/setupcubetask.h"
#include "task/vulkan/setuptextedittask.h"
#include "task/vulkan/setuptexttask.h"
#include "task/window/initwindowtask.h"
#include "task/window/processinputeventtask.h"
#include "task/window/runwindowtask.h"
#include "task/window/windoweventsignal.h"

#include <memory>

using namespace vigine;

std::unique_ptr<TaskFlow> createInitTaskFlow(MouseEventSignalBinder &mouseSignalBinder,
                                             KeyEventSignalBinder &keySignalBinder,
                                             std::shared_ptr<TextEditState> textEditState,
                                             std::shared_ptr<TextEditorSystem> textEditorSystem)
{
    auto taskFlow    = std::make_unique<TaskFlow>();

    auto *initWindow = taskFlow->addTask(std::make_unique<InitWindowTask>());
    auto *initVulkan = taskFlow->addTask(std::make_unique<InitVulkanTask>());
    auto *setupCube  = taskFlow->addTask(std::make_unique<SetupCubeTask>());
    auto *setupText  = taskFlow->addTask(std::make_unique<SetupTextTask>());
    auto *setupTextEdit =
        taskFlow->addTask(std::make_unique<SetupTextEditTask>(textEditState, textEditorSystem));
    auto *runWindow             = taskFlow->addTask(std::make_unique<RunWindowTask>());
    auto *processInputEventTask = taskFlow->addTask(std::make_unique<ProcessInputEventTask>());

    auto *runWindowTask         = static_cast<RunWindowTask *>(runWindow);
    runWindowTask->setTextEditorSystem(std::move(textEditorSystem));

    static_cast<void>(taskFlow->route(initWindow, initVulkan));
    static_cast<void>(taskFlow->route(initVulkan, setupCube));
    static_cast<void>(taskFlow->route(setupCube, setupText));
    static_cast<void>(taskFlow->route(setupText, setupTextEdit));
    static_cast<void>(taskFlow->route(setupTextEdit, runWindow));
    static_cast<void>(taskFlow->signal(runWindow, processInputEventTask, &mouseSignalBinder));
    static_cast<void>(taskFlow->signal(runWindow, processInputEventTask, &keySignalBinder));

    taskFlow->changeCurrentTaskTo(initWindow);

    return taskFlow;
}

std::unique_ptr<TaskFlow> createWorkTaskFlow()
{
    auto taskFlow    = std::make_unique<TaskFlow>();

    auto *renderCube = taskFlow->addTask(std::make_unique<RenderCubeTask>());
    taskFlow->changeCurrentTaskTo(renderCube);

    return taskFlow;
}

std::unique_ptr<TaskFlow> createErrorTaskFlow() { return std::make_unique<TaskFlow>(); }

std::unique_ptr<TaskFlow> createCloseTaskFlow() { return std::make_unique<TaskFlow>(); }

int main()
{
    Engine engine;
    StateMachine *stMachine = engine.state();

    MouseEventSignalBinder mouseSignalBinder;
    KeyEventSignalBinder keySignalBinder;

    auto textEditState    = std::make_shared<TextEditState>();
    auto textEditorSystem = std::make_shared<TextEditorSystem>(textEditState);

    auto initState        = std::make_unique<InitState>();
    auto workState        = std::make_unique<WorkState>();
    auto errorState       = std::make_unique<ErrorState>();
    auto closeState       = std::make_unique<CloseState>();

    auto initPtr          = stMachine->addState(std::move(initState));
    auto workPtr          = stMachine->addState(std::move(workState));
    auto errorPtr         = stMachine->addState(std::move(errorState));
    auto closePtr         = stMachine->addState(std::move(closeState));

    initPtr->setTaskFlow(
        createInitTaskFlow(mouseSignalBinder, keySignalBinder, textEditState, textEditorSystem));
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

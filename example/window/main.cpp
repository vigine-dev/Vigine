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
#include "task/vulkan/loadtexturestask.h"
#include "task/vulkan/rendercubetask.h"
#include "task/vulkan/setupcubetask.h"
#include "task/vulkan/setuphelpergeometrytask.h"
#include "task/vulkan/setuptextedittask.h"
#include "task/vulkan/setuptexttask.h"
#include "task/vulkan/setuptexturedplanestask.h"
#include "task/window/initwindowtask.h"
#include "task/window/processinputeventtask.h"
#include "task/window/runwindowtask.h"
#include "task/window/windoweventpayload.h"

#include <vigine/payload/factory.h>
#include <vigine/payload/ipayloadregistry.h>
#include <vigine/payload/payloadtypeid.h>
#include <vigine/signalemitter/defaultsignalemitter.h>
#include <vigine/signalemitter/isignalemitter.h>
#include <vigine/threading/factory.h>
#include <vigine/threading/ithreadmanager.h>
#include <vigine/threading/threadaffinity.h>

#include <cassert>
#include <memory>


using namespace vigine;

std::unique_ptr<TaskFlow> createInitTaskFlow(signalemitter::ISignalEmitter *signalEmitter,
                                             std::shared_ptr<TextEditState> textEditState,
                                             std::shared_ptr<TextEditorSystem> textEditorSystem)
{
    auto taskFlow             = std::make_unique<TaskFlow>();

    auto *initWindow          = taskFlow->addTask(std::make_unique<InitWindowTask>());
    auto *initVulkan          = taskFlow->addTask(std::make_unique<InitVulkanTask>());
    auto *setupHelperGeometry = taskFlow->addTask(std::make_unique<SetupHelperGeometryTask>());
    auto *setupCube           = taskFlow->addTask(std::make_unique<SetupCubeTask>());
    auto *setupText           = taskFlow->addTask(std::make_unique<SetupTextTask>());
    auto *loadTextures        = taskFlow->addTask(std::make_unique<LoadTexturesTask>());
    auto *setupTexturedPlanes = taskFlow->addTask(std::make_unique<SetupTexturedPlanesTask>());
    auto *setupTextEdit =
        taskFlow->addTask(std::make_unique<SetupTextEditTask>(textEditState, textEditorSystem));
    auto *runWindow             = taskFlow->addTask(std::make_unique<RunWindowTask>());
    auto *processInputEventTask = taskFlow->addTask(std::make_unique<ProcessInputEventTask>());

    auto *runWindowTask         = static_cast<RunWindowTask *>(runWindow);
    runWindowTask->setTextEditorSystem(std::move(textEditorSystem));
    runWindowTask->setSignalEmitter(signalEmitter);

    taskFlow->setSignalEmitter(signalEmitter);

    static_cast<void>(taskFlow->route(initWindow, initVulkan));
    static_cast<void>(taskFlow->route(initVulkan, setupHelperGeometry));
    static_cast<void>(taskFlow->route(setupHelperGeometry, setupCube));
    static_cast<void>(taskFlow->route(setupCube, setupText));
    static_cast<void>(taskFlow->route(setupText, loadTextures));
    static_cast<void>(taskFlow->route(loadTextures, setupTexturedPlanes));
    static_cast<void>(taskFlow->route(setupTexturedPlanes, setupTextEdit));
    static_cast<void>(taskFlow->route(setupTextEdit, runWindow));
    // ThreadAffinity::Any keeps the subscriber wired directly to the bus.
    // The emitter is built with sharedBusConfig() below, so the shared
    // bus drains dispatch on a pool worker and the handler still lands
    // off the Win32 pump thread. ThreadAffinity::Pool would require the
    // TaskFlow's context to expose IThreadManager, which the legacy
    // vigine::Engine does not wire; sharedBusConfig + Any reaches the
    // same "handler off the pump thread" outcome without touching the
    // engine's construction chain.
    static_cast<void>(taskFlow->signal(runWindow, processInputEventTask,
                                       kMouseButtonDownPayloadTypeId,
                                       threading::ThreadAffinity::Any));
    static_cast<void>(taskFlow->signal(runWindow, processInputEventTask,
                                       kKeyDownPayloadTypeId,
                                       threading::ThreadAffinity::Any));

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
    // Thread manager owns the pool that drains the shared bus below. It
    // must outlive the emitter and is declared before the Engine so its
    // destructor runs last in the reverse-of-declaration order.
    auto threadManager    = vigine::threading::createThreadManager();

    // Payload registry is stack-local for the example. Register the two
    // user-range payload ids used by the window input pipeline; the
    // engine-bundled ranges (Control, System, SystemExt, Reserved) are
    // pre-registered by the factory so no wiring is needed for them.
    auto payloadRegistry  = vigine::payload::createPayloadRegistry();
    static_cast<void>(payloadRegistry->registerRange(
        kMouseButtonDownPayloadTypeId, kMouseButtonDownPayloadTypeId,
        "example-window.mouse"));
    static_cast<void>(payloadRegistry->registerRange(
        kKeyDownPayloadTypeId, kKeyDownPayloadTypeId,
        "example-window.key"));

    // Shared-pool bus so dispatch to ProcessInputEventTask::onMessage
    // lands on a pool worker, off the Win32 message pump thread. The
    // TaskFlow::signal wiring below uses ThreadAffinity::Any; the
    // cross-thread hop is provided by the bus policy, not by
    // ScheduledDelivery.
    auto signalEmitter    = vigine::signalemitter::createSignalEmitter(
        *threadManager, vigine::signalemitter::sharedBusConfig());

    Engine engine;
    // Engine::state() now returns IStateMachine&; downcast back to the
    // concrete StateMachine for the rich state-machine API. The cast
    // is temporary scaffolding that will disappear once addState /
    // addTransition move onto the interface in a later leaf.
    StateMachine *stMachine = dynamic_cast<StateMachine *>(&engine.state());
    assert(stMachine != nullptr &&
           "Engine::state() must be a concrete StateMachine until the "
           "rich API lifts onto IStateMachine");

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
        createInitTaskFlow(signalEmitter.get(), textEditState, textEditorSystem));
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

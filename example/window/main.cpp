#include "texteditstate.h"

#include <vigine/impl/statemachine/statemachine.h>
#include <vigine/impl/taskflow/taskflow.h>
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

#include <vigine/api/messaging/payload/factory.h>
#include <vigine/api/messaging/payload/ipayloadregistry.h>
#include <vigine/api/messaging/payload/payloadtypeid.h>
#include <vigine/impl/messaging/signalemitter.h>
#include <vigine/api/messaging/isignalemitter.h>
#include <vigine/core/threading/ithreadmanager.h>
#include <vigine/core/threading/threadaffinity.h>

#include <vigine/context.h>

#include <cassert>
#include <memory>


using namespace vigine;

std::unique_ptr<TaskFlow> createInitTaskFlow(messaging::ISignalEmitter *signalEmitter,
                                             Context *context,
                                             std::shared_ptr<TextEditState> textEditState,
                                             std::shared_ptr<TextEditorSystem> textEditorSystem)
{
    auto taskFlow             = std::make_unique<TaskFlow>();
    // Task flow needs the concrete legacy Context so its signal()
    // non-Any branch can reach the engine-owned IThreadManager (see
    // TaskFlow::signal and the accompanying code comment). The
    // downcast happens once in main(); here we simply install it.
    taskFlow->setContext(context);

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
    // Pool affinity wraps the subscriber in a scheduled-delivery adapter
    // that hands the clone to IThreadManager::schedule. The engine's
    // Context owns a real IThreadManager (Engine::Engine plumbs it on
    // construction), and main() hands that concrete Context* into
    // createInitTaskFlow, which installs it via taskFlow->setContext(context)
    // above. Input handlers therefore run on a pool worker thread, off
    // the Win32 message pump thread, and clicking the window does not
    // stall rendering if the handler grows heavier later.
    static_cast<void>(taskFlow->signal(runWindow, processInputEventTask,
                                       kMouseButtonDownPayloadTypeId,
                                       core::threading::ThreadAffinity::Pool));
    static_cast<void>(taskFlow->signal(runWindow, processInputEventTask,
                                       kKeyDownPayloadTypeId,
                                       core::threading::ThreadAffinity::Pool));

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
    // The engine owns the thread manager and wires it through its
    // Context on construction. The signal emitter and the TaskFlow's
    // non-Any signal path both reach into that single manager, so the
    // pool bank is shared across the whole example.
    Engine engine;

    // Engine::state() returns IStateMachine&; downcast back to the
    // concrete StateMachine for the rich state-machine API. The cast
    // is temporary scaffolding that will disappear once addState /
    // addTransition move onto the interface in a later change.
    StateMachine *stMachine = dynamic_cast<StateMachine *>(&engine.state());
    assert(stMachine != nullptr &&
           "Engine::state() must be a concrete StateMachine until the "
           "rich API lifts onto IStateMachine");

    // Engine::context() returns IContext&; the task flow still takes
    // the concrete legacy Context*, so downcast once here.
    Context *legacyCtx = dynamic_cast<Context *>(&engine.context());
    assert(legacyCtx != nullptr &&
           "Engine::context() must be backed by the legacy Context until "
           "the task flow migrates to the IContext surface");

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
    // bus and the TaskFlow::signal non-Any scheduler both share the
    // single IThreadManager that the engine plumbs through its
    // Context — see TaskFlow::signal and Engine::Engine.
    auto signalEmitter    = vigine::messaging::createSignalEmitter(
        engine.context().threadManager(),
        vigine::messaging::sharedBusConfig());

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
        createInitTaskFlow(signalEmitter.get(), legacyCtx, textEditState, textEditorSystem));
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

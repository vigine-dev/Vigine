// example-window
//
// Drives the modern FSM-pumped engine through a four-state machine
// (Init -> Work -> Close, with Error as the failure off-ramp). Each state
// owns a TaskFlow registered through @c IStateMachine::addStateTaskFlow;
// the engine pump advances the active state's flow one task per tick and
// drains queued FSM transitions on the controller thread.
//
// Modern wiring overview:
//   1. Build the engine via @c vigine::engine::createEngine. The engine
//      owns the @c IContext aggregator (thread manager + system bus +
//      Level-1 wrappers).
//   2. Build the legacy @c EntityManager + the platform / graphics
//      subsystems (RenderSystem, WindowSystem). The legacy substrate
//      stays here because the modern @c IECS handle still uses
//      EntityId-typed entries while the rendering pipeline below expects
//      legacy @c Entity* handles.
//   3. Build the platform / graphics services on the modern
//      @c vigine::service::AbstractService base, attach their underlying
//      systems through @c setRenderSystem / @c setWindowSystem, and
//      register them on the engine context. The aggregator stamps a
//      generational @c ServiceId on each registration; the example
//      captures the stamped ids on the wrappers and hands them to every
//      task that needs the service.
//   4. Build the user-bus signal emitter against the engine-owned thread
//      manager so input events fan out off the platform message-pump
//      thread.
//   5. Allocate four FSM states, build a TaskFlow per state (init flow
//      drives the Vulkan boot path + window pump; work flow ticks the
//      cube; close / error flows are no-ops), and register each via
//      @c addStateTaskFlow.
//   6. Wire transitions, install one state-bound TaskFlow per state, set
//      the initial state, and call @c IEngine::run.

#include "texteditstate.h"

#include <vigine/api/engine/engineconfig.h>
#include <vigine/api/engine/factory.h>
#include <vigine/api/engine/iengine.h>
#include <vigine/api/context/icontext.h>
#include <vigine/api/messaging/factory.h>
#include <vigine/api/messaging/payload/factory.h>
#include <vigine/api/messaging/payload/ipayloadregistry.h>
#include <vigine/api/messaging/payload/payloadtypeid.h>
#include <vigine/api/service/serviceid.h>
#include <vigine/api/statemachine/istatemachine.h>
#include <vigine/api/statemachine/stateid.h>
#include <vigine/core/threading/ithreadmanager.h>
#include <vigine/core/threading/threadaffinity.h>
#include <vigine/impl/ecs/entitymanager.h>
#include <vigine/impl/ecs/graphics/graphicsservice.h>
#include <vigine/impl/ecs/graphics/rendersystem.h>
#include <vigine/impl/ecs/platform/platformservice.h>
#include <vigine/impl/ecs/platform/windowsystem.h>
#include <vigine/impl/messaging/signalemitter.h>
#include <vigine/impl/taskflow/taskflow.h>

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

#include <iostream>
#include <memory>
#include <utility>

namespace
{
struct InitFlowDeps
{
    vigine::EntityManager *entityManager{nullptr};
    vigine::service::ServiceId platformServiceId{};
    vigine::service::ServiceId graphicsServiceId{};
    vigine::engine::IEngine *engine{nullptr};
    vigine::messaging::ISignalEmitter *signalEmitter{nullptr};
    std::shared_ptr<TextEditState> textEditState{};
    std::shared_ptr<TextEditorSystem> textEditorSystem{};
};

std::unique_ptr<vigine::TaskFlow> createInitTaskFlow(const InitFlowDeps &deps)
{
    auto taskFlow             = std::make_unique<vigine::TaskFlow>();

    auto initWindowOwned      = std::make_unique<InitWindowTask>();
    initWindowOwned->setEntityManager(deps.entityManager);
    initWindowOwned->setPlatformServiceId(deps.platformServiceId);
    auto *initWindow          = taskFlow->addTask(std::move(initWindowOwned));

    auto initVulkanOwned      = std::make_unique<InitVulkanTask>();
    initVulkanOwned->setEntityManager(deps.entityManager);
    initVulkanOwned->setPlatformServiceId(deps.platformServiceId);
    initVulkanOwned->setGraphicsServiceId(deps.graphicsServiceId);
    auto *initVulkan          = taskFlow->addTask(std::move(initVulkanOwned));

    auto setupHelperOwned     = std::make_unique<SetupHelperGeometryTask>();
    setupHelperOwned->setEntityManager(deps.entityManager);
    setupHelperOwned->setGraphicsServiceId(deps.graphicsServiceId);
    auto *setupHelperGeometry = taskFlow->addTask(std::move(setupHelperOwned));

    auto setupCubeOwned       = std::make_unique<SetupCubeTask>();
    setupCubeOwned->setEntityManager(deps.entityManager);
    setupCubeOwned->setGraphicsServiceId(deps.graphicsServiceId);
    auto *setupCube           = taskFlow->addTask(std::move(setupCubeOwned));

    auto setupTextOwned       = std::make_unique<SetupTextTask>();
    setupTextOwned->setEntityManager(deps.entityManager);
    setupTextOwned->setGraphicsServiceId(deps.graphicsServiceId);
    auto *setupText           = taskFlow->addTask(std::move(setupTextOwned));

    auto loadTexturesOwned    = std::make_unique<LoadTexturesTask>();
    loadTexturesOwned->setEntityManager(deps.entityManager);
    loadTexturesOwned->setGraphicsServiceId(deps.graphicsServiceId);
    auto *loadTextures        = taskFlow->addTask(std::move(loadTexturesOwned));

    auto setupPlanesOwned     = std::make_unique<SetupTexturedPlanesTask>();
    setupPlanesOwned->setEntityManager(deps.entityManager);
    setupPlanesOwned->setGraphicsServiceId(deps.graphicsServiceId);
    auto *setupTexturedPlanes = taskFlow->addTask(std::move(setupPlanesOwned));

    auto setupTextEditOwned   = std::make_unique<SetupTextEditTask>(
        deps.textEditState, deps.textEditorSystem);
    setupTextEditOwned->setEntityManager(deps.entityManager);
    setupTextEditOwned->setGraphicsServiceId(deps.graphicsServiceId);
    auto *setupTextEdit       = taskFlow->addTask(std::move(setupTextEditOwned));

    auto runWindowOwned       = std::make_unique<RunWindowTask>();
    runWindowOwned->setEntityManager(deps.entityManager);
    runWindowOwned->setPlatformServiceId(deps.platformServiceId);
    runWindowOwned->setGraphicsServiceId(deps.graphicsServiceId);
    runWindowOwned->setEngine(deps.engine);
    runWindowOwned->setTextEditorSystem(deps.textEditorSystem);
    runWindowOwned->setSignalEmitter(deps.signalEmitter);
    auto *runWindow           = taskFlow->addTask(std::move(runWindowOwned));

    auto *processInputOwned   = taskFlow->addTask(std::make_unique<ProcessInputEventTask>());

    taskFlow->setSignalEmitter(deps.signalEmitter);

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
    // context owns the IThreadManager and the TaskFlow's signal path
    // routes through ISignalEmitter, so input handlers run on a pool
    // worker thread, off the Win32 message-pump thread, and clicking
    // the window does not stall rendering if the handler grows heavier
    // later.
    static_cast<void>(taskFlow->signal(runWindow, processInputOwned,
                                       kMouseButtonDownPayloadTypeId,
                                       vigine::core::threading::ThreadAffinity::Pool));
    static_cast<void>(taskFlow->signal(runWindow, processInputOwned,
                                       kKeyDownPayloadTypeId,
                                       vigine::core::threading::ThreadAffinity::Pool));

    taskFlow->changeCurrentTaskTo(initWindow);

    return taskFlow;
}

std::unique_ptr<vigine::TaskFlow> createWorkTaskFlow()
{
    auto taskFlow    = std::make_unique<vigine::TaskFlow>();

    auto *renderCube = taskFlow->addTask(std::make_unique<RenderCubeTask>());
    taskFlow->changeCurrentTaskTo(renderCube);

    return taskFlow;
}

std::unique_ptr<vigine::TaskFlow> createErrorTaskFlow()
{
    return std::make_unique<vigine::TaskFlow>();
}

std::unique_ptr<vigine::TaskFlow> createCloseTaskFlow()
{
    return std::make_unique<vigine::TaskFlow>();
}

} // namespace

int main()
{
    // Modern engine. Owns the IContext aggregator with thread manager,
    // system bus, IECS / IStateMachine / ITaskFlow Level-1 wrappers, and
    // the service registry.
    auto engine = vigine::engine::createEngine();
    if (!engine)
    {
        std::cerr << "[example-window] createEngine returned null" << std::endl;
        return 1;
    }

    auto &context  = engine->context();
    auto &fsm      = context.stateMachine();

    // Legacy substrate the rendering / window pipeline still needs
    // (RenderSystem + WindowSystem keep using @c Entity* handles).
    auto entityManager = std::make_unique<vigine::EntityManager>();
    auto windowSystem  = std::make_unique<vigine::ecs::platform::WindowSystem>("MainWindow");
    auto renderSystem  = std::make_unique<vigine::ecs::graphics::RenderSystem>("MainRender");

    // Modern services. Each derives from
    // @c vigine::service::AbstractService and exposes its underlying
    // legacy system through a setter; the example wires the systems in
    // before registration so the stamped ServiceId is the only handle
    // tasks need to reach the wrappers.
    auto platformService =
        std::make_shared<vigine::ecs::platform::PlatformService>(vigine::Name("MainPlatform"));
    platformService->setWindowSystem(windowSystem.get());

    auto graphicsService =
        std::make_shared<vigine::ecs::graphics::GraphicsService>(vigine::Name("MainGraphics"));
    graphicsService->setRenderSystem(renderSystem.get());

    if (auto regResult = context.registerService(platformService); regResult.isError())
    {
        std::cerr << "[example-window] failed to register PlatformService: "
                  << regResult.message() << std::endl;
        return 1;
    }
    if (auto regResult = context.registerService(graphicsService); regResult.isError())
    {
        std::cerr << "[example-window] failed to register GraphicsService: "
                  << regResult.message() << std::endl;
        return 1;
    }

    const vigine::service::ServiceId platformServiceId = platformService->id();
    const vigine::service::ServiceId graphicsServiceId = graphicsService->id();

    // Payload registry for the user-range payload ids used by the
    // window input pipeline. Engine-bundled ranges (Control, System,
    // SystemExt, Reserved) are pre-registered by the factory.
    auto payloadRegistry  = vigine::payload::createPayloadRegistry();
    static_cast<void>(payloadRegistry->registerRange(
        kMouseButtonDownPayloadTypeId, kMouseButtonDownPayloadTypeId,
        "example-window.mouse"));
    static_cast<void>(payloadRegistry->registerRange(
        kKeyDownPayloadTypeId, kKeyDownPayloadTypeId,
        "example-window.key"));

    // Shared-pool signal emitter so dispatch lands on a pool worker, off
    // the Win32 message-pump thread. Built against the engine-owned
    // thread manager so the bus shares one worker pool across the whole
    // example.
    auto signalEmitter = vigine::messaging::createSignalEmitter(
        context.threadManager(), vigine::messaging::sharedBusConfig());
    if (!signalEmitter)
    {
        std::cerr << "[example-window] createSignalEmitter returned null" << std::endl;
        return 1;
    }

    auto textEditState    = std::make_shared<TextEditState>();
    auto textEditorSystem = std::make_shared<TextEditorSystem>(textEditState);

    // FSM states: Init -> Work -> Close, Error as the failure off-ramp.
    // Reuse the FSM's auto-provisioned default state as InitState so
    // every later @c addState call returns a fresh handle with a known
    // generation. The default state already comes selected as the
    // initial state per UD-3, so no explicit setInitial is needed when
    // the example reuses it.
    const vigine::statemachine::StateId initState  = fsm.current();
    const vigine::statemachine::StateId workState  = fsm.addState();
    const vigine::statemachine::StateId errorState = fsm.addState();
    const vigine::statemachine::StateId closeState = fsm.addState();

    // Bind a TaskFlow per state through the modern FSM-drive surface.
    InitFlowDeps initDeps{};
    initDeps.entityManager     = entityManager.get();
    initDeps.platformServiceId = platformServiceId;
    initDeps.graphicsServiceId = graphicsServiceId;
    initDeps.engine            = engine.get();
    initDeps.signalEmitter     = signalEmitter.get();
    initDeps.textEditState     = textEditState;
    initDeps.textEditorSystem  = textEditorSystem;

    if (auto regResult = fsm.addStateTaskFlow(initState, createInitTaskFlow(initDeps));
        regResult.isError())
    {
        std::cerr << "[example-window] addStateTaskFlow(init) failed: "
                  << regResult.message() << std::endl;
        return 1;
    }
    if (auto regResult = fsm.addStateTaskFlow(workState, createWorkTaskFlow());
        regResult.isError())
    {
        std::cerr << "[example-window] addStateTaskFlow(work) failed: "
                  << regResult.message() << std::endl;
        return 1;
    }
    if (auto regResult = fsm.addStateTaskFlow(errorState, createErrorTaskFlow());
        regResult.isError())
    {
        std::cerr << "[example-window] addStateTaskFlow(error) failed: "
                  << regResult.message() << std::endl;
        return 1;
    }
    if (auto regResult = fsm.addStateTaskFlow(closeState, createCloseTaskFlow());
        regResult.isError())
    {
        std::cerr << "[example-window] addStateTaskFlow(close) failed: "
                  << regResult.message() << std::endl;
        return 1;
    }

    // Run the engine. @c IEngine::run blocks the calling thread on the
    // main-thread pump until shutdown is observed; the InitState's
    // RunWindowTask handles the long-running platform showWindow call,
    // and on @c WM_CLOSE the task returns success which cascades the
    // FSM -> WorkState -> CloseState. The CloseState flow is empty so
    // @c hasTasksToRun() reports false and the engine pump idles until
    // we ask it to shut down explicitly.
    if (auto runResult = engine->run(); runResult.isError())
    {
        std::cerr << "[example-window] engine->run failed: " << runResult.message()
                  << std::endl;
        return 1;
    }

    return 0;
}

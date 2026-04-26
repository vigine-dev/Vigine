// example-window
//
// Demonstrates the modern FSM-pumped engine where IContext is the
// self-sufficient task execution environment. The engine builds every
// default (entity manager, signal emitter, platform / graphics services)
// during context construction so a host program only has to:
//   1. createEngine() to spin up the context aggregator + the lifecycle
//      primitives;
//   2. Register any application-scope services (the example registers
//      its TextEditorService under example::services::wellknown::textEditor);
//   3. Allocate FSM states and bind a TaskFlow per state — the tasks
//      themselves resolve every dependency through @c apiToken() in
//      their run() method, so no setters are threaded through the wiring
//      path.
//
// Applications that need different concrete platform / graphics
// implementations replace the engine defaults via
// IContext::registerService(myService, vigine::service::wellknown::*);
// the unique_ptr / shared_ptr slot's RAII chain destroys the prior
// occupant in place.

#include <vigine/api/context/icontext.h>
#include <vigine/api/engine/engineconfig.h>
#include <vigine/api/engine/factory.h>
#include <vigine/api/engine/iengine.h>
#include <vigine/api/messaging/payload/factory.h>
#include <vigine/api/messaging/payload/ipayloadregistry.h>
#include <vigine/api/messaging/payload/payloadtypeid.h>
#include <vigine/api/statemachine/istatemachine.h>
#include <vigine/api/statemachine/stateid.h>
#include <vigine/api/taskflow/factory.h>
#include <vigine/api/taskflow/itaskflow.h>
#include <vigine/api/taskflow/resultcode.h>
#include <vigine/api/taskflow/taskid.h>
#include <vigine/result.h>

#include "services/texteditorservice.h"
#include "services/wellknown.h"
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

std::unique_ptr<vigine::taskflow::ITaskFlow> createInitTaskFlow()
{
    using vigine::taskflow::ResultCode;
    using vigine::taskflow::TaskId;

    auto taskFlow = vigine::taskflow::createTaskFlow();

    // Each runnable resolves every dependency through its own apiToken()
    // inside run(); no setters are threaded through here. The flow
    // therefore receives default-constructed unique_ptrs and only owns
    // the runnables thereafter.
    const TaskId initWindowId    = taskFlow->addTask(std::make_unique<InitWindowTask>());
    const TaskId initVulkanId    = taskFlow->addTask(std::make_unique<InitVulkanTask>());
    const TaskId setupHelperId   = taskFlow->addTask(std::make_unique<SetupHelperGeometryTask>());
    const TaskId setupCubeId     = taskFlow->addTask(std::make_unique<SetupCubeTask>());
    const TaskId setupTextId     = taskFlow->addTask(std::make_unique<SetupTextTask>());
    const TaskId loadTexturesId  = taskFlow->addTask(std::make_unique<LoadTexturesTask>());
    const TaskId setupPlanesId   = taskFlow->addTask(std::make_unique<SetupTexturedPlanesTask>());
    const TaskId setupTextEditId = taskFlow->addTask(std::make_unique<SetupTextEditTask>());
    const TaskId runWindowId     = taskFlow->addTask(std::make_unique<RunWindowTask>());
    const TaskId processInputId  = taskFlow->addTask(std::make_unique<ProcessInputEventTask>());

    static_cast<void>(taskFlow->route(initWindowId,    initVulkanId));
    static_cast<void>(taskFlow->route(initVulkanId,    setupHelperId));
    static_cast<void>(taskFlow->route(setupHelperId,   setupCubeId));
    static_cast<void>(taskFlow->route(setupCubeId,     setupTextId));
    static_cast<void>(taskFlow->route(setupTextId,     loadTexturesId));
    static_cast<void>(taskFlow->route(loadTexturesId,  setupPlanesId));
    static_cast<void>(taskFlow->route(setupPlanesId,   setupTextEditId));
    static_cast<void>(taskFlow->route(setupTextEditId, runWindowId));

    using vigine::core::threading::ThreadAffinity;
    static_cast<void>(taskFlow->signal(runWindowId, processInputId,
                                       kMouseButtonDownPayloadTypeId,
                                       ThreadAffinity::Pool));
    static_cast<void>(taskFlow->signal(runWindowId, processInputId,
                                       kKeyDownPayloadTypeId,
                                       ThreadAffinity::Pool));

    static_cast<void>(taskFlow->setRoot(initWindowId));

    return taskFlow;
}

std::unique_ptr<vigine::taskflow::ITaskFlow> createWorkTaskFlow()
{
    auto taskFlow = vigine::taskflow::createTaskFlow();

    const vigine::taskflow::TaskId renderCubeId =
        taskFlow->addTask(std::make_unique<RenderCubeTask>());
    static_cast<void>(taskFlow->setRoot(renderCubeId));

    return taskFlow;
}

std::unique_ptr<vigine::taskflow::ITaskFlow> createErrorTaskFlow()
{
    return vigine::taskflow::createTaskFlow();
}

std::unique_ptr<vigine::taskflow::ITaskFlow> createCloseTaskFlow()
{
    return vigine::taskflow::createTaskFlow();
}

} // namespace

int main()
{
    auto engine = vigine::engine::createEngine();
    if (!engine)
    {
        std::cerr << "[example-window] createEngine returned null" << std::endl;
        return 1;
    }

    auto &context = engine->context();
    auto &fsm     = context.stateMachine();

    // Application-scope service registration. The engine already has
    // defaults for platform / graphics services + entity manager +
    // signal emitter; the example only needs to add its own text
    // editor service under an app-side well-known id.
    if (auto regResult = context.registerService(
            std::make_shared<TextEditorService>(),
            example::services::wellknown::textEditor);
        regResult.isError())
    {
        std::cerr << "[example-window] failed to register TextEditorService: "
                  << regResult.message() << std::endl;
        return 1;
    }

    // Payload registry for the user-range payload ids used by the
    // window input pipeline. Engine-bundled ranges (Control, System,
    // SystemExt, Reserved) are pre-registered by the factory.
    auto payloadRegistry = vigine::payload::createPayloadRegistry();
    static_cast<void>(payloadRegistry->registerRange(
        kMouseButtonDownPayloadTypeId, kMouseButtonDownPayloadTypeId,
        "example-window.mouse"));
    static_cast<void>(payloadRegistry->registerRange(
        kKeyDownPayloadTypeId, kKeyDownPayloadTypeId,
        "example-window.key"));

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

    if (auto regResult = fsm.addStateTaskFlow(initState, createInitTaskFlow());
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

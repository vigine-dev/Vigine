#include "runwindowtask.h"

#include <vigine/api/context/icontext.h>
#include <vigine/api/ecs/ientitymanager.h>
#include <vigine/api/engine/iengine.h>
#include <vigine/api/engine/iengine_token.h>
#include <vigine/api/messaging/isignalemitter.h>
#include <vigine/api/messaging/isubscriptiontoken.h>
#include <vigine/api/service/wellknown.h>
#include <vigine/impl/ecs/entitymanager.h>
#include <vigine/impl/ecs/graphics/graphicsservice.h>
#include <vigine/impl/ecs/graphics/rendersystem.h>
#include <vigine/impl/ecs/platform/platformservice.h>

#include "../../handler/windoweventhandler.h"
#include "../../services/texteditorservice.h"
#include "../../services/wellknown.h"
#include "impl/ecs/platform/windowcomponent.h"
#include "windoweventpayload.h"

#include <iostream>
#include <memory>

#ifdef _WIN32
#include "impl/ecs/platform/winapicomponent.h"

#include <windows.h>

#endif

namespace
{

// Resolves the editor service through the engine token and ensures
// it is wired (idempotent). Returns @c nullptr when the token is
// missing, the service is unavailable, or the type cast fails — the
// caller skips its work.
TextEditorService *resolveTextEditorService(vigine::engine::IEngineToken &token)
{
    auto result = token.service(example::services::wellknown::textEditor);
    if (!result.ok())
        return nullptr;
    auto *service = dynamic_cast<TextEditorService *>(&result.value());
    if (!service)
        return nullptr;
    service->ensureWired(token.engine().context());
    return service;
}

vigine::ecs::platform::PlatformService *
resolvePlatformService(vigine::engine::IEngineToken &token)
{
    auto result = token.service(vigine::service::wellknown::platformService);
    if (!result.ok())
        return nullptr;
    return dynamic_cast<vigine::ecs::platform::PlatformService *>(&result.value());
}

vigine::EntityManager *resolveEntityManager(vigine::engine::IEngineToken &token)
{
    auto result = token.entityManager();
    if (!result.ok())
        return nullptr;
    return dynamic_cast<vigine::EntityManager *>(&result.value());
}

} // namespace

RunWindowTask::RunWindowTask() = default;

vigine::Result RunWindowTask::run()
{
    auto *token = apiToken();
    if (!token)
        return vigine::Result(vigine::Result::Code::Error, "Engine token is unavailable");

    auto *entityManager   = resolveEntityManager(*token);
    auto *platformService = resolvePlatformService(*token);
    auto *editorService   = resolveTextEditorService(*token);
    if (!entityManager || !platformService || !editorService)
        return vigine::Result(vigine::Result::Code::Error,
                              "RunWindowTask: failed to resolve engine dependencies");

    // Subscribe a one-shot expiration callback the FIRST time the task
    // runs. The callback flips _shutdownRequested and asks the platform
    // service to close the active windows so the blocking showWindow
    // call unwinds and run() returns.
    if (!_expirationSubscription)
    {
        _expirationSubscription = token->subscribeExpiration(
            [this]() { _shutdownRequested.store(true, std::memory_order_release); });
    }

    auto *entity = entityManager->getEntityByAlias("MainWindow");
    if (!entity)
        return vigine::Result(vigine::Result::Code::Error, "MainWindow entity not found");

    auto windows = platformService->windowComponents(entity);
    if (windows.empty())
        return vigine::Result(vigine::Result::Code::Error, "Window component is unavailable");

    for (std::size_t windowIndex = 0; windowIndex < windows.size(); ++windowIndex)
    {
        auto *window = windows[windowIndex];
        if (!window)
            return vigine::Result(vigine::Result::Code::Error,
                                  "Window component is unavailable");

        auto eventHandlers = platformService->windowEventHandlers(entity, window);
        if (eventHandlers.empty())
            return vigine::Result(vigine::Result::Code::Error,
                                  "Window event handler is unavailable");

        for (auto *eventHandler : eventHandlers)
        {
            auto *windowEventHandler = dynamic_cast<WindowEventHandler *>(eventHandler);
            if (!windowEventHandler)
                return vigine::Result(vigine::Result::Code::Error,
                                      "Window event handler has unsupported type");

            windowEventHandler->setMouseButtonDownCallback(
                [this](vigine::ecs::platform::MouseButton button, int x, int y) {
                    onMouseButtonDown(button, x, y);
                });
            windowEventHandler->setMouseButtonUpCallback(
                [this](vigine::ecs::platform::MouseButton button, int x, int y) {
                    onMouseButtonUp(button, x, y);
                });
            windowEventHandler->setMouseMoveCallback(
                [this](int x, int y) { onMouseMove(x, y); });
            windowEventHandler->setMouseWheelCallback(
                [this](int delta, int x, int y) { onMouseWheel(delta, x, y); });
            windowEventHandler->setKeyDownCallback(
                [this](const vigine::ecs::platform::KeyEvent &event) { onKeyDown(event); });
            windowEventHandler->setKeyUpCallback(
                [this](const vigine::ecs::platform::KeyEvent &event) { onKeyUp(event); });
            windowEventHandler->setCharCallback(
                [this](const vigine::ecs::platform::TextEvent &event) { onChar(event); });
            windowEventHandler->setWindowResizedCallback([this](int width, int height) {
                if (auto *t = apiToken())
                    if (auto *svc = resolveTextEditorService(*t))
                        svc->onWindowResized(width, height);
            });
        }

        std::cout << "[RunWindowTask] Showing window " << (windowIndex + 1) << std::endl;
        window->setFrameCallback([this, window]() {
            // Token expired -- post a WM_CLOSE so the platform message
            // loop in @c WinAPIComponent::show observes a quit and
            // unwinds. Lock-free check; bounds the FSM-transition →
            // window-close latency to one frame.
            if (_shutdownRequested.load(std::memory_order_acquire))
            {
#ifdef _WIN32
                if (auto *winApiWindow =
                        dynamic_cast<vigine::ecs::platform::WinAPIComponent *>(window))
                {
                    if (auto *handle = static_cast<HWND>(winApiWindow->nativeHandle()))
                        PostMessageW(handle, WM_CLOSE, 0, 0);
                }
#else
                static_cast<void>(window);
#endif
                return;
            }

            auto *frameToken = apiToken();
            if (!frameToken)
                return;

            // Editor onFrame: applies any pending swapchain resize
            // (debounced 80 ms by the system), drives cursor blink,
            // rebuilds the editor mesh on dirty state.
            if (auto *svc = resolveTextEditorService(*frameToken))
                svc->onFrame();

            // Render the next frame against the (possibly just-resized)
            // surface.
            auto graphicsResult =
                frameToken->service(vigine::service::wellknown::graphicsService);
            if (graphicsResult.ok())
            {
                if (auto *gs = dynamic_cast<vigine::ecs::graphics::GraphicsService *>(
                        &graphicsResult.value()))
                {
                    if (auto *rs = gs->renderSystem())
                    {
                        rs->update();
#ifdef _WIN32
                        if (auto *winApiWindow =
                                dynamic_cast<vigine::ecs::platform::WinAPIComponent *>(window))
                            winApiWindow->setRenderedVertexCount(
                                rs->lastRenderedVertexCount());
#endif
                    }
                }
            }
        });
        auto showResult = platformService->showWindow(window);
        if (showResult.isError())
            return showResult;
        std::cout << "[RunWindowTask] Window " << (windowIndex + 1) << " closed, continuing"
                  << std::endl;
    }

    // Drop the expiration subscription explicitly so a late FSM
    // invalidation that arrives between this point and dtor cannot
    // dereference any stale handle below. The dtor would do this
    // anyway, but ordering keeps the contract obvious.
    _expirationSubscription.reset();

    // Ask the engine to stop the main pump so @c IEngine::run returns
    // to @c main and the process exits cleanly.
    token->engine().shutdown();

    return vigine::Result();
}

void RunWindowTask::onMouseButtonDown(vigine::ecs::platform::MouseButton button, int x, int y)
{
    auto *token = apiToken();
    if (!token)
        return;

    if (auto *svc = resolveTextEditorService(*token))
        svc->onMouseButtonDown(button, x, y);

    // Fan-out the input event onto the signal bus so subscribers
    // (e.g. @c ProcessInputEventTask) observe it through the
    // engine's signal-emitter facade.
    static_cast<void>(token->signalEmitter().emit(
        std::make_unique<MouseButtonDownPayload>(button, x, y)));
}

void RunWindowTask::onMouseButtonUp(vigine::ecs::platform::MouseButton button, int x, int y)
{
    auto *token = apiToken();
    if (!token)
        return;

    if (auto *svc = resolveTextEditorService(*token))
        svc->onMouseButtonUp(button, x, y);
}

void RunWindowTask::onMouseMove(int x, int y)
{
    auto *token = apiToken();
    if (!token)
        return;

    if (auto *svc = resolveTextEditorService(*token))
        svc->onMouseMove(x, y);
}

void RunWindowTask::onMouseWheel(int delta, int x, int y)
{
    auto *token = apiToken();
    if (!token)
        return;

    if (auto *svc = resolveTextEditorService(*token))
        svc->onMouseWheel(delta, x, y);
}

void RunWindowTask::onKeyDown(const vigine::ecs::platform::KeyEvent &event)
{
    auto *token = apiToken();
    if (!token)
        return;

    if (auto *svc = resolveTextEditorService(*token))
        svc->onKeyDown(event);

    static_cast<void>(token->signalEmitter().emit(std::make_unique<KeyDownPayload>(event)));
}

void RunWindowTask::onKeyUp(const vigine::ecs::platform::KeyEvent &event)
{
    auto *token = apiToken();
    if (!token)
        return;

    if (auto *svc = resolveTextEditorService(*token))
        svc->onKeyUp(event);
}

void RunWindowTask::onChar(const vigine::ecs::platform::TextEvent &event)
{
    auto *token = apiToken();
    if (!token)
        return;

    if (auto *svc = resolveTextEditorService(*token))
        svc->onChar(event);
}

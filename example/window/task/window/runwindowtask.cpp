#include "runwindowtask.h"

#include <vigine/api/context/icontext.h>
#include <vigine/api/ecs/ientitymanager.h>
#include <vigine/api/engine/iengine.h>
#include <vigine/api/engine/iengine_token.h>
#include <vigine/api/messaging/isignalemitter.h>
#include <vigine/api/messaging/isubscriptiontoken.h>
#include <vigine/api/service/wellknown.h>
#include "vigine/impl/ecs/entity.h"
#include "vigine/impl/ecs/entitymanager.h"
#include "vigine/impl/ecs/graphics/meshcomponent.h"
#include "vigine/impl/ecs/graphics/rendercomponent.h"
#include "vigine/impl/ecs/graphics/rendersystem.h"
#include "vigine/impl/ecs/graphics/shadercomponent.h"
#include "vigine/impl/ecs/graphics/transformcomponent.h"
#include <vigine/impl/ecs/graphics/graphicsservice.h>
#include <vigine/impl/ecs/platform/platformservice.h>

#include "../../handler/windoweventhandler.h"
#include "../../services/texteditorservice.h"
#include "../../services/wellknown.h"
#include "../../system/texteditorsystem.h"
#include "impl/ecs/platform/windowcomponent.h"
#include "windoweventpayload.h"

#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>
#include <memory>

#ifdef _WIN32
#include "impl/ecs/platform/winapicomponent.h"

#include <windows.h>

#endif

namespace
{
constexpr unsigned int kKeyW               = 'W';
constexpr unsigned int kKeyA               = 'A';
constexpr unsigned int kKeyS               = 'S';
constexpr unsigned int kKeyD               = 'D';
constexpr unsigned int kKeyQ               = 'Q';
constexpr unsigned int kKeyE               = 'E';
constexpr unsigned int kKeyShift           = 0x10;
constexpr unsigned int kKeyLeftShift       = 0xA0;
constexpr unsigned int kKeyRightShift      = 0xA1;
constexpr unsigned int kKeyControl         = 0x11;
constexpr unsigned int kKeyLeftControl     = 0xA2;
constexpr unsigned int kKeyRightControl    = 0xA3;
constexpr unsigned int kKeyAlt             = 0x12;
constexpr unsigned int kKeyLeftAlt         = 0xA4;
constexpr unsigned int kKeyRightAlt        = 0xA5;
constexpr unsigned int kKeyRayToggle       = 'R';
constexpr unsigned int kKeyBillboardToggle = 'B';
constexpr unsigned int kKeyEscape          = 0x1B;

#ifdef _WIN32
std::string utf8FromWide(const wchar_t *wide)
{
    if (!wide)
        return {};

    const int needed = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1)
        return {};

    std::string result(static_cast<size_t>(needed - 1), '\0');
    static_cast<void>(
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, result.data(), needed, nullptr, nullptr));
    return result;
}

std::wstring wideFromUtf8(const std::string &utf8)
{
    if (utf8.empty())
        return {};

    const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (needed <= 1)
        return {};

    std::wstring result(static_cast<size_t>(needed - 1), L'\0');
    static_cast<void>(MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, result.data(), needed));
    return result;
}
#endif

// ---------------------------------------------------------------------------
// Local resolution helpers — pure functions over an engine token. Each helper
// re-reads the relevant slot from @ref vigine::IContext on every call; no
// task-side caching. Helpers fail soft (return @c nullptr) when the gated
// accessor reports @c Expired / @c NotFound or when a dynamic_cast turns out
// negative — the caller branches on the null and skips its work without
// surfacing an error to the FSM.
// ---------------------------------------------------------------------------

vigine::EntityManager *resolveEntityManager(vigine::engine::IEngineToken &token)
{
    auto result = token.entityManager();
    if (!result.ok())
        return nullptr;
    return dynamic_cast<vigine::EntityManager *>(&result.value());
}

vigine::ecs::platform::PlatformService *
resolvePlatformService(vigine::engine::IEngineToken &token)
{
    auto result = token.service(vigine::service::wellknown::platformService);
    if (!result.ok())
        return nullptr;
    return dynamic_cast<vigine::ecs::platform::PlatformService *>(&result.value());
}

vigine::ecs::graphics::GraphicsService *
resolveGraphicsService(vigine::engine::IEngineToken &token)
{
    auto result = token.service(vigine::service::wellknown::graphicsService);
    if (!result.ok())
        return nullptr;
    return dynamic_cast<vigine::ecs::graphics::GraphicsService *>(&result.value());
}

vigine::ecs::graphics::RenderSystem *
resolveRenderSystem(vigine::engine::IEngineToken &token)
{
    auto *gs = resolveGraphicsService(token);
    return gs ? gs->renderSystem() : nullptr;
}

std::shared_ptr<TextEditorSystem>
resolveTextEditor(vigine::engine::IEngineToken &token)
{
    auto result = token.service(example::services::wellknown::textEditor);
    if (!result.ok())
        return nullptr;
    auto *service = dynamic_cast<TextEditorService *>(&result.value());
    if (!service)
        return nullptr;
    // ensureWired is idempotent — first call binds the underlying
    // TextEditorSystem to the engine's entity manager + graphics
    // service / render system; every subsequent call is a no-op.
    service->ensureWired(token.engine().context());
    return service->textEditorSystem();
}

} // namespace

RunWindowTask::RunWindowTask() {}

// COPILOT_TODO: Guarantee unbindEntity() on every early exit via an
// RAII/scope guard; otherwise the underlying RenderSystem/WindowSystem
// can stay bound to the entity after an error return path.
vigine::Result RunWindowTask::run()
{
    auto *token = apiToken();
    if (!token)
        return vigine::Result(vigine::Result::Code::Error, "Engine token is unavailable");

    auto *entityManager   = resolveEntityManager(*token);
    auto *platformService = resolvePlatformService(*token);
    auto *graphicsService = resolveGraphicsService(*token);
    auto *renderSystem    = graphicsService ? graphicsService->renderSystem() : nullptr;
    if (!entityManager || !platformService || !graphicsService || !renderSystem)
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

    static_cast<void>(ensureMouseRayEntity());
    static_cast<void>(ensureMouseClickSphereEntity());

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
                    std::cout << "[RunWindowTask::run::lambda] button="
                              << static_cast<int>(button) << ", x=" << x << ", y=" << y
                              << std::endl;
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
                [this](const vigine::ecs::platform::KeyEvent &event) {
                    if (!event.isRepeat)
                        std::cout
                            << "[RunWindowTask::run::lambda] keyCode=" << event.keyCode
                            << ", scanCode=" << event.scanCode
                            << ", modifiers=" << event.modifiers
                            << ", repeatCount=" << event.repeatCount
                            << ", isRepeat=" << event.isRepeat << std::endl;
                    onKeyDown(event);
                });
            windowEventHandler->setKeyUpCallback(
                [this](const vigine::ecs::platform::KeyEvent &event) { onKeyUp(event); });
            windowEventHandler->setCharCallback(
                [this](const vigine::ecs::platform::TextEvent &event) { onChar(event); });
            windowEventHandler->setWindowResizedCallback([this, window](int width, int height) {
                onWindowResized(window, width, height);
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

            // Per-frame deps resolution. Same engine-default services
            // resolve every tick; the lookup is cheap (registry hash
            // + dynamic_cast) and keeps the "no caches" rule visible.
            auto *frameToken = apiToken();
            if (!frameToken)
                return;

            auto *frameRenderSystem = resolveRenderSystem(*frameToken);
            auto  frameTextEditor   = resolveTextEditor(*frameToken);

            bool resizedThisTick = false;

            if (_resizePending && frameRenderSystem)
            {
                const auto now    = std::chrono::steady_clock::now();
                const bool paused = now - _lastResizeEvent >= std::chrono::milliseconds(80);
                if (paused)
                {
                    if (_pendingResizeWidth != _appliedResizeWidth ||
                        _pendingResizeHeight != _appliedResizeHeight)
                    {
                        const bool resized =
                            frameRenderSystem->resize(_pendingResizeWidth, _pendingResizeHeight);
                        if (!resized)
                        {
                            std::cerr << "[RunWindowTask] Failed to recreate swapchain on "
                                         "resize: "
                                      << _pendingResizeWidth << "x" << _pendingResizeHeight
                                      << std::endl;
                        } else
                        {
                            _appliedResizeWidth  = _pendingResizeWidth;
                            _appliedResizeHeight = _pendingResizeHeight;
                            _lastResizeApply     = now;
                            resizedThisTick      = true;
                        }
                    }

                    _resizePending = false;
                }
            }

            if (frameTextEditor)
                frameTextEditor->onFrame();

            if (frameRenderSystem && !resizedThisTick)
            {
                frameRenderSystem->update();
#ifdef _WIN32
                if (auto *winApiWindow =
                        dynamic_cast<vigine::ecs::platform::WinAPIComponent *>(window))
                    winApiWindow->setRenderedVertexCount(
                        frameRenderSystem->lastRenderedVertexCount());
#endif
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

    auto *renderSystem  = resolveRenderSystem(*token);
    auto  textEditor    = resolveTextEditor(*token);
    auto *signalEmitter = &token->signalEmitter();

    if (button == vigine::ecs::platform::MouseButton::Left)
    {
        vigine::Entity *picked =
            renderSystem ? renderSystem->pickFirstIntersectedEntity(x, y) : nullptr;

        _lastMouseRayX     = x;
        _lastMouseRayY     = y;
        _hasMouseRaySample = true;

        const bool pickedTextEditor = textEditor && textEditor->isEditorEntity(picked);

        // Freeze click marker first, then build ray from the same click point.
        updateMouseClickSphereVisualization(x, y);
        updateMouseRayVisualization(x, y);

        bool consumedByScrollbar = false;
        if (pickedTextEditor && textEditor)
            consumedByScrollbar = textEditor->onMouseButtonDown(x, y, picked);

        if (pickedTextEditor && textEditor && !consumedByScrollbar)
            textEditor->onEditorClick(x, y);

        // In Ctrl camera-unlock mode keep current focus unchanged.
        if (!_ctrlHeld)
        {
            // Restore normal behavior: clicked entity receives focus, including text editor.
            setFocusedEntity(picked);

            if (_focusedEntity)
            {
                _movementKeyMask = 0;
                if (renderSystem)
                {
                    renderSystem->setMoveForwardActive(false);
                    renderSystem->setMoveBackwardActive(false);
                    renderSystem->setMoveLeftActive(false);
                    renderSystem->setMoveRightActive(false);
                    renderSystem->setMoveUpActive(false);
                    renderSystem->setMoveDownActive(false);
                    renderSystem->setSprintActive(false);
                }
            }
        }
    }

    if (renderSystem && button == vigine::ecs::platform::MouseButton::Right &&
        (!_focusedEntity || _ctrlHeld || _objectDragActive))
        renderSystem->beginCameraDrag(x, y);

    std::cout << "[RunWindowTask::onMouseButtonDown] button=" << static_cast<int>(button)
              << ", x=" << x << ", y=" << y << std::endl;
    static_cast<void>(signalEmitter->emit(
        std::make_unique<MouseButtonDownPayload>(button, x, y)));
}

void RunWindowTask::onMouseButtonUp(vigine::ecs::platform::MouseButton button, int x, int y)
{
    static_cast<void>(x);
    static_cast<void>(y);

    auto *token = apiToken();
    if (!token)
        return;

    auto *renderSystem = resolveRenderSystem(*token);
    auto  textEditor   = resolveTextEditor(*token);

    if (button == vigine::ecs::platform::MouseButton::Left && textEditor)
        textEditor->onMouseButtonUp();

    if (renderSystem && button == vigine::ecs::platform::MouseButton::Right &&
        (!_focusedEntity || _ctrlHeld || _objectDragActive))
        renderSystem->endCameraDrag();
}

void RunWindowTask::onMouseMove(int x, int y)
{
    _lastMouseRayX     = x;
    _lastMouseRayY     = y;
    _hasMouseRaySample = true;

    auto *token = apiToken();
    if (!token)
        return;

    auto *renderSystem = resolveRenderSystem(*token);
    auto  textEditor   = resolveTextEditor(*token);

    if (_objectDragActive)
        updateObjectDrag(x, y);

    if (textEditor)
        textEditor->onMouseMove(x, y);

    if (renderSystem && (!_focusedEntity || _ctrlHeld || _objectDragActive))
        renderSystem->updateCameraDrag(x, y);
}

void RunWindowTask::onMouseWheel(int delta, int x, int y)
{
    auto *token = apiToken();
    if (!token)
        return;

    auto *renderSystem = resolveRenderSystem(*token);
    auto  textEditor   = resolveTextEditor(*token);

    // In object-drag mode: wheel adjusts object distance from camera.
    if (_objectDragActive)
    {
        const float wheelSteps  = static_cast<float>(delta) / 120.0f;
        const float factor      = std::pow(0.90f, -wheelSteps);
        _dragDistanceFromCamera = (std::max)(0.15f, _dragDistanceFromCamera * factor);
        // Allow Z movement so the object actually moves in depth, not just XY.
        updateObjectDrag(x, y, /*suppressZDelta=*/false);
        return;
    }

    // When text editor is focused (and Ctrl not held): scroll text, not camera.
    if (_focusedEntity && !_ctrlHeld && textEditor)
    {
        textEditor->onMouseWheel(delta);
        return;
    }

    if (renderSystem && (!_focusedEntity || _ctrlHeld))
        renderSystem->zoomCamera(delta);
}

void RunWindowTask::onKeyDown(const vigine::ecs::platform::KeyEvent &event)
{
    auto *token = apiToken();
    if (!token)
        return;

    auto *renderSystem  = resolveRenderSystem(*token);
    auto  textEditor    = resolveTextEditor(*token);
    auto *signalEmitter = &token->signalEmitter();

    if (event.keyCode == kKeyControl || event.keyCode == kKeyLeftControl ||
        event.keyCode == kKeyRightControl)
        _ctrlHeld = true;

    if (!event.isRepeat &&
        (event.keyCode == kKeyAlt || event.keyCode == kKeyLeftAlt || event.keyCode == kKeyRightAlt))
    {
        if (_objectDragActive)
        {
            endObjectDrag();
        } else if (_focusedEntity)
        {
            const int mx = _hasMouseRaySample ? _lastMouseRayX : 0;
            const int my = _hasMouseRaySample ? _lastMouseRayY : 0;
            static_cast<void>(beginObjectDrag(_focusedEntity, mx, my));
        }
        return;
    }

    if (event.keyCode == kKeyRayToggle && !event.isRepeat)
    {
        _mouseRayVisible = !_mouseRayVisible;

        if (_mouseRayVisible)
        {
            if (_hasMouseRaySample)
                updateMouseRayVisualization(_lastMouseRayX, _lastMouseRayY);
        } else if (ensureMouseRayEntity() && renderSystem)
        {
            renderSystem->bindEntity(_mouseRayEntity);
            if (auto *rc = renderSystem->boundRenderComponent())
            {
                auto transform = rc->getTransform();
                transform.setPosition({0.0f, -100.0f, 0.0f});
                transform.setScale({0.01f, 0.01f, 0.01f});
                rc->setTransform(transform);
            }
            renderSystem->unbindEntity();
        }
    }

    if (event.keyCode == kKeyBillboardToggle && !event.isRepeat)
    {
        if (renderSystem)
            renderSystem->toggleBillboard();
    }

    if (!_focusedEntity || _ctrlHeld || _objectDragActive)
        updateCameraMovementKey(event.keyCode, true);

    if (event.keyCode == kKeyEscape)
    {
        setFocusedEntity(nullptr);
        return;
    }

    if (handleClipboardShortcut(event))
        return;

    if (textEditor && isFocusedTextEditor())
        textEditor->onKeyDown(event.keyCode);

    if (!event.isRepeat)
        std::cout << "[RunWindowTask::onKeyDown] keyCode=" << event.keyCode
                  << ", scanCode=" << event.scanCode << std::endl;
    static_cast<void>(signalEmitter->emit(std::make_unique<KeyDownPayload>(event)));
}

void RunWindowTask::onKeyUp(const vigine::ecs::platform::KeyEvent &event)
{
    if (event.keyCode == kKeyControl || event.keyCode == kKeyLeftControl ||
        event.keyCode == kKeyRightControl)
        _ctrlHeld = false;

    if (!_focusedEntity || _ctrlHeld || _objectDragActive)
        updateCameraMovementKey(event.keyCode, false);
}

void RunWindowTask::updateCameraMovementKey(unsigned int keyCode, bool pressed)
{
    auto *token = apiToken();
    if (!token)
        return;
    auto *renderSystem = resolveRenderSystem(*token);
    if (!renderSystem)
        return;

    auto setMoveMaskBit = [this, pressed](uint8_t bit) {
        if (pressed)
            _movementKeyMask = static_cast<uint8_t>(_movementKeyMask | bit);
        else
            _movementKeyMask = static_cast<uint8_t>(_movementKeyMask & ~bit);
    };

    switch (keyCode)
    {
    case kKeyW:
        renderSystem->setMoveForwardActive(pressed);
        setMoveMaskBit(MoveKeyW);
        break;
    case kKeyS:
        renderSystem->setMoveBackwardActive(pressed);
        setMoveMaskBit(MoveKeyS);
        break;
    case kKeyA:
        renderSystem->setMoveLeftActive(pressed);
        setMoveMaskBit(MoveKeyA);
        break;
    case kKeyD:
        renderSystem->setMoveRightActive(pressed);
        setMoveMaskBit(MoveKeyD);
        break;
    case kKeyQ:
        renderSystem->setMoveDownActive(pressed);
        setMoveMaskBit(MoveKeyQ);
        break;
    case kKeyE:
        renderSystem->setMoveUpActive(pressed);
        setMoveMaskBit(MoveKeyE);
        break;
    case kKeyShift:
    case kKeyLeftShift:
    case kKeyRightShift:
        renderSystem->setSprintActive(pressed);
        break;
    default:
        break;
    }
}

void RunWindowTask::onWindowResized(vigine::ecs::platform::WindowComponent *window, int width,
                                    int height)
{
    if (!window)
        return;

    if (width <= 0 || height <= 0)
        return;

    _pendingResizeWindow = window;
    _pendingResizeWidth  = static_cast<uint32_t>(width);
    _pendingResizeHeight = static_cast<uint32_t>(height);
    _resizePending       = true;
    _lastResizeEvent     = std::chrono::steady_clock::now();
}

void RunWindowTask::onChar(const vigine::ecs::platform::TextEvent &event)
{
    auto *token = apiToken();
    if (!token)
        return;
    auto textEditor = resolveTextEditor(*token);
    if (textEditor && isFocusedTextEditor())
        textEditor->onChar(event, _movementKeyMask);
}

bool RunWindowTask::handleClipboardShortcut(const vigine::ecs::platform::KeyEvent &event)
{
    auto *token = apiToken();
    if (!token)
        return false;
    auto textEditor = resolveTextEditor(*token);
    if (!textEditor || !isFocusedTextEditor())
        return false;

    const bool ctrlPressed = (event.modifiers & vigine::ecs::platform::KeyModifierControl) != 0;
    if (!ctrlPressed)
        return false;

    if (event.keyCode == 'C' || event.keyCode == 'X')
    {
#ifdef _WIN32
        const std::wstring wide = wideFromUtf8(textEditor->text());
        if (!wide.empty() && OpenClipboard(nullptr))
        {
            static_cast<void>(EmptyClipboard());
            const SIZE_T bytes = (wide.size() + 1) * sizeof(wchar_t);
            HGLOBAL memory     = GlobalAlloc(GMEM_MOVEABLE, bytes);
            if (memory)
            {
                void *ptr = GlobalLock(memory);
                if (ptr)
                {
                    std::memcpy(ptr, wide.c_str(), bytes);
                    GlobalUnlock(memory);
                    static_cast<void>(SetClipboardData(CF_UNICODETEXT, memory));
                    memory = nullptr;
                }
            }
            if (memory)
                GlobalFree(memory);
            CloseClipboard();
        }
#endif

        if (event.keyCode == 'X')
            textEditor->clearText();
        return true;
    }

    if (event.keyCode == 'V')
    {
#ifdef _WIN32
        if (OpenClipboard(nullptr))
        {
            HANDLE data = GetClipboardData(CF_UNICODETEXT);
            if (data)
            {
                auto *wide = static_cast<const wchar_t *>(GlobalLock(data));
                if (wide)
                {
                    textEditor->insertUtf8(utf8FromWide(wide));
                    GlobalUnlock(data);
                }
            }
            CloseClipboard();
        }
#endif
        return true;
    }

    return false;
}

bool RunWindowTask::isFocusedTextEditor() const
{
    if (!_focusedEntity)
        return false;
    auto *token = const_cast<RunWindowTask *>(this)->apiToken();
    if (!token)
        return false;
    auto textEditor = resolveTextEditor(*token);
    if (!textEditor)
        return false;
    return textEditor->isEditorEntity(_focusedEntity);
}

void RunWindowTask::setFocusedEntity(vigine::Entity *entity)
{
    if (entity == _focusedEntity)
        return;

    auto *token = apiToken();
    if (!token)
        return;
    auto *renderSystem = resolveRenderSystem(*token);
    auto  textEditor   = resolveTextEditor(*token);

    // Any focus change exits move mode.
    if (_objectDragActive)
        endObjectDrag();

    if (_focusedEntity && renderSystem && _hasFocusedOriginalScale)
    {
        renderSystem->bindEntity(_focusedEntity);
        if (auto *rc = renderSystem->boundRenderComponent())
        {
            auto transform = rc->getTransform();
            transform.setScale(_focusedOriginalScale);
            rc->setTransform(transform);
        }
        renderSystem->unbindEntity();
    }

    _focusedEntity           = entity;
    _hasFocusedOriginalScale = false;

    // Editor entities receive focus for input but should not get the scale-up
    // visual effect.
    const bool isEditor = textEditor && textEditor->isEditorEntity(entity);

    if (textEditor)
        textEditor->setFocused(_focusedEntity != nullptr && isEditor);

    if (_focusedEntity && renderSystem && !isEditor)
    {
        renderSystem->bindEntity(_focusedEntity);
        if (auto *rc = renderSystem->boundRenderComponent())
        {
            auto transform        = rc->getTransform();
            _focusedOriginalScale = transform.getScale();
            transform.setScale(_focusedOriginalScale * 1.08f);
            rc->setTransform(transform);
            _hasFocusedOriginalScale = true;
        }
        renderSystem->unbindEntity();
    }
}

bool RunWindowTask::beginObjectDrag(vigine::Entity *entity, int x, int y)
{
    if (!entity)
        return false;
    auto *token = apiToken();
    if (!token)
        return false;
    auto *renderSystem = resolveRenderSystem(*token);
    auto  textEditor   = resolveTextEditor(*token);
    if (!renderSystem)
        return false;

    renderSystem->bindEntity(entity);
    auto *rc = renderSystem->boundRenderComponent();
    if (!rc)
    {
        renderSystem->unbindEntity();
        return false;
    }

    const auto transform = rc->getTransform();
    renderSystem->unbindEntity();

    glm::vec3 rayOrigin(0.0f);
    glm::vec3 rayDirection(0.0f);
    if (!renderSystem->screenPointToRayFromNearPlane(x, y, rayOrigin, rayDirection))
        return false;

    const float dirLen = glm::length(rayDirection);
    if (dirLen < 1e-6f)
        return false;
    rayDirection            /= dirLen;

    _dragDistanceFromCamera  = glm::dot(transform.getPosition() - rayOrigin, rayDirection);
    if (_dragDistanceFromCamera <= 0.0f)
        return false;

    const glm::vec3 hit = rayOrigin + rayDirection * _dragDistanceFromCamera;

    _objectDragActive   = true;
    _dragEditorGroup    = (textEditor && textEditor->isEditorEntity(entity));
    _dragEntity         = entity;
    _dragGrabOffset     = transform.getPosition() - hit;
    return true;
}

void RunWindowTask::updateObjectDrag(int x, int y, bool suppressZDelta)
{
    if (!_objectDragActive || !_dragEntity)
        return;

    auto *token = apiToken();
    if (!token)
        return;
    auto *entityManager = resolveEntityManager(*token);
    auto *renderSystem  = resolveRenderSystem(*token);
    auto  textEditor    = resolveTextEditor(*token);
    if (!renderSystem)
        return;

    glm::vec3 rayOrigin(0.0f);
    glm::vec3 rayDirection(0.0f);
    if (!renderSystem->screenPointToRayFromNearPlane(x, y, rayOrigin, rayDirection))
        return;

    const float dirLen = glm::length(rayDirection);
    if (dirLen < 1e-6f)
        return;
    rayDirection           /= dirLen;

    const glm::vec3 hit     = rayOrigin + rayDirection * _dragDistanceFromCamera;
    const glm::vec3 newPos  = hit + _dragGrabOffset;

    renderSystem->bindEntity(_dragEntity);
    auto *dragRc = renderSystem->boundRenderComponent();
    if (!dragRc)
    {
        renderSystem->unbindEntity();
        return;
    }

    auto dragTr     = dragRc->getTransform();
    glm::vec3 delta = newPos - dragTr.getPosition();
    if (_dragEditorGroup)
    {
        if (suppressZDelta)
            delta.z = 0.0f; // lateral mouse move: keep editor layer depths stable
        else
        {
            delta.x = 0.0f; // depth (wheel): no XY drift, only Z moves
            delta.y = 0.0f;
        }
    }

    dragTr.setPosition(dragTr.getPosition() + delta);
    dragRc->setTransform(dragTr);

    // If dragging an editor entity, update glyph vertices for TextEditEntity.
    if (_dragEditorGroup)
        dragRc->translateGlyphVertices(delta);

    renderSystem->unbindEntity();

    if (_dragEditorGroup && entityManager)
    {
        const char *editorAliases[] = {
            "TextEditBgEntity",
            "TextEditEntity",
            "TextEditScrollbarTrackEntity",
            "TextEditScrollbarThumbEntity",
            "TextEditFocusTopEntity",
            "TextEditFocusBottomEntity",
            "TextEditFocusLeftEntity",
            "TextEditFocusRightEntity",
        };

        for (const char *alias : editorAliases)
        {
            auto *e = entityManager->getEntityByAlias(alias);
            if (!e || e == _dragEntity)
                continue;

            renderSystem->bindEntity(e);
            if (auto *rc = renderSystem->boundRenderComponent())
            {
                auto tr = rc->getTransform();
                tr.setPosition(tr.getPosition() + delta);
                rc->setTransform(tr);

                if (std::strcmp(alias, "TextEditEntity") == 0)
                    rc->translateGlyphVertices(delta);
            }
            renderSystem->unbindEntity();
        }

        if (textEditor)
        {
            textEditor->offsetEditorFrame(delta.x, delta.y, delta.z);
            textEditor->refreshEditorLayout();
        }
        renderSystem->markGlyphDirty();
    }
}

void RunWindowTask::endObjectDrag()
{
    _objectDragActive       = false;
    _dragEditorGroup        = false;
    _dragEntity             = nullptr;
    _dragDistanceFromCamera = 0.0f;
    _dragGrabOffset         = {0.0f, 0.0f, 0.0f};
}

bool RunWindowTask::ensureMouseRayEntity()
{
    if (_mouseRayEntity)
        return true;

    auto *token = apiToken();
    if (!token)
        return false;
    auto *entityManager = resolveEntityManager(*token);
    auto *renderSystem  = resolveRenderSystem(*token);
    if (!entityManager || !renderSystem)
        return false;

    _mouseRayEntity = entityManager->getEntityByAlias("MouseRayEntity");
    if (!_mouseRayEntity)
    {
        _mouseRayEntity = entityManager->createEntity();
        if (!_mouseRayEntity)
            return false;

        entityManager->addAlias(_mouseRayEntity, "MouseRayEntity");
    }

    renderSystem->createComponents(_mouseRayEntity);
    renderSystem->bindEntity(_mouseRayEntity);
    auto *rc = renderSystem->boundRenderComponent();
    if (!rc)
    {
        renderSystem->unbindEntity();
        return false;
    }

    auto rayMesh = vigine::ecs::graphics::MeshComponent::createCube();
    rayMesh.setProceduralInShader(true, 36);
    rc->setMesh(rayMesh);
    {
        vigine::ecs::graphics::ShaderComponent shader("cube.vert.spv", "cube.frag.spv");
        rc->setShader(shader);
    }
    rc->setPickable(false);

    vigine::ecs::graphics::TransformComponent transform;
    transform.setPosition({0.0f, -100.0f, 0.0f});
    transform.setScale({0.01f, 0.01f, 0.01f});
    rc->setTransform(transform);

    renderSystem->unbindEntity();
    return true;
}

bool RunWindowTask::ensureMouseClickSphereEntity()
{
    if (_mouseClickSphereEntity)
        return true;

    auto *token = apiToken();
    if (!token)
        return false;
    auto *entityManager = resolveEntityManager(*token);
    auto *renderSystem  = resolveRenderSystem(*token);
    if (!entityManager || !renderSystem)
        return false;

    _mouseClickSphereEntity = entityManager->getEntityByAlias("MouseClickSphereEntity");
    if (!_mouseClickSphereEntity)
    {
        _mouseClickSphereEntity = entityManager->createEntity();
        if (!_mouseClickSphereEntity)
            return false;

        entityManager->addAlias(_mouseClickSphereEntity, "MouseClickSphereEntity");
    }

    renderSystem->createComponents(_mouseClickSphereEntity);
    renderSystem->bindEntity(_mouseClickSphereEntity);
    auto *rc = renderSystem->boundRenderComponent();
    if (!rc)
    {
        renderSystem->unbindEntity();
        return false;
    }

    auto sphereMesh = vigine::ecs::graphics::MeshComponent::createCube();
    sphereMesh.setProceduralInShader(true, 768); // Sphere shader generates 768 vertices
    rc->setMesh(sphereMesh);
    {
        vigine::ecs::graphics::ShaderComponent shader("sphere.vert.spv", "sphere.frag.spv");
        rc->setShader(shader);
    }
    rc->setPickable(false);

    vigine::ecs::graphics::TransformComponent transform;
    transform.setPosition({0.0f, -100.0f, 0.0f});
    transform.setScale({0.06f, 0.06f, 0.06f});
    rc->setTransform(transform);

    renderSystem->unbindEntity();
    return true;
}

void RunWindowTask::updateMouseRayVisualization(int x, int y)
{
    auto *token = apiToken();
    if (!token)
        return;
    auto *renderSystem = resolveRenderSystem(*token);
    if (!renderSystem)
        return;

    if (!ensureMouseRayEntity())
        return;

    glm::vec3 clickRayOrigin(0.0f);
    glm::vec3 clickRayDirection(0.0f);
    if (!renderSystem->screenPointToRayFromNearPlane(x, y, clickRayOrigin, clickRayDirection))
        return;

    const glm::vec3 rayDirection  = glm::normalize(clickRayDirection);

    constexpr float kRayLength    = 60.0f;
    constexpr float kRayThickness = 0.012f;
    constexpr float kStartOffset  = 0.03f;

    // Start and direction are both derived from clicked screen pixel.
    const glm::vec3 rayStart      = clickRayOrigin + clickRayDirection * kStartOffset;

    const glm::vec3 center        = rayStart + rayDirection * (kRayLength * 0.5f);
    const glm::quat orientation   = glm::rotation(glm::vec3(0.0f, 0.0f, 1.0f), rayDirection);
    const glm::vec3 rotationEuler = glm::eulerAngles(orientation);

    renderSystem->bindEntity(_mouseRayEntity);
    if (auto *rc = renderSystem->boundRenderComponent())
    {
        auto transform = rc->getTransform();
        if (_mouseRayVisible)
        {
            transform.setPosition(center);
            transform.setRotation(rotationEuler);
            transform.setScale({kRayThickness, kRayThickness, kRayLength});
        } else
        {
            transform.setPosition({0.0f, -100.0f, 0.0f});
            transform.setScale({0.01f, 0.01f, 0.01f});
        }
        rc->setTransform(transform);
    }
    renderSystem->unbindEntity();
}

void RunWindowTask::updateMouseClickSphereVisualization(int x, int y)
{
    auto *token = apiToken();
    if (!token)
        return;
    auto *renderSystem = resolveRenderSystem(*token);
    if (!renderSystem)
        return;

    if (!ensureMouseClickSphereEntity())
        return;

    glm::vec3 clickRayOrigin(0.0f);
    glm::vec3 clickRayDirection(0.0f);
    if (!renderSystem->screenPointToRayFromNearPlane(x, y, clickRayOrigin, clickRayDirection))
        return;

    constexpr float kStartOffset = 0.03f;
    const glm::vec3 sphereCenter = clickRayOrigin + clickRayDirection * kStartOffset;

    renderSystem->bindEntity(_mouseClickSphereEntity);
    if (auto *rc = renderSystem->boundRenderComponent())
    {
        auto transform = rc->getTransform();
        transform.setPosition(sphereCenter);
        transform.setScale({0.05f, 0.05f, 0.05f});
        rc->setTransform(transform);
    }
    renderSystem->unbindEntity();
}

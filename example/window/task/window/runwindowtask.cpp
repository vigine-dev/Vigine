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

} // namespace

RunWindowTask::RunWindowTask() {}

RunWindowTask::Deps RunWindowTask::resolveDeps() const
{
    Deps deps;

    auto *token = const_cast<RunWindowTask *>(this)->apiToken();
    if (!token)
        return deps;

    auto entityManagerResult = token->entityManager();
    if (entityManagerResult.ok())
        deps.entityManager =
            dynamic_cast<vigine::EntityManager *>(&entityManagerResult.value());

    auto platformResult = token->service(vigine::service::wellknown::platformService);
    if (platformResult.ok())
        deps.platformService = dynamic_cast<vigine::ecs::platform::PlatformService *>(
            &platformResult.value());

    auto graphicsResult = token->service(vigine::service::wellknown::graphicsService);
    if (graphicsResult.ok())
    {
        deps.graphicsService = dynamic_cast<vigine::ecs::graphics::GraphicsService *>(
            &graphicsResult.value());
        if (deps.graphicsService)
            deps.renderSystem = deps.graphicsService->renderSystem();
    }

    deps.signalEmitter = &token->signalEmitter();
    deps.engine        = &token->engine();

    auto editorSvcResult = token->service(example::services::wellknown::textEditor);
    if (editorSvcResult.ok())
    {
        if (auto *editorService =
                dynamic_cast<TextEditorService *>(&editorSvcResult.value()))
        {
            // ensureWired is idempotent — first call binds the
            // underlying TextEditorSystem to the entity manager and
            // graphics service, every subsequent call is a no-op.
            editorService->ensureWired(deps.engine->context());
            deps.textEditorSystem = editorService->system();
        }
    }

    return deps;
}

// COPILOT_TODO: Guarantee unbindEntity() on every early exit via an
// RAII/scope guard; otherwise the underlying RenderSystem/WindowSystem
// can stay bound to the entity after an error return path.
vigine::Result RunWindowTask::run()
{
    auto deps = resolveDeps();
    if (!deps.entityManager || !deps.platformService || !deps.graphicsService ||
        !deps.renderSystem || !deps.engine)
        return vigine::Result(vigine::Result::Code::Error,
                              "RunWindowTask: failed to resolve engine dependencies");

    // Subscribe a one-shot expiration callback the FIRST time the task
    // runs. The callback flips _shutdownRequested and asks the platform
    // service to close the active windows so the blocking showWindow
    // call unwinds and run() returns.
    if (!_expirationSubscription)
    {
        if (auto *token = apiToken())
        {
            _expirationSubscription = token->subscribeExpiration(
                [this]() { _shutdownRequested.store(true, std::memory_order_release); });
        }
    }

    auto *entity = deps.entityManager->getEntityByAlias("MainWindow");
    if (!entity)
        return vigine::Result(vigine::Result::Code::Error, "MainWindow entity not found");
    _mainWindowEntity = entity;

    static_cast<void>(ensureMouseRayEntity(deps));
    static_cast<void>(ensureMouseClickSphereEntity(deps));

    auto windows = deps.platformService->windowComponents(entity);
    if (windows.empty())
        return vigine::Result(vigine::Result::Code::Error, "Window component is unavailable");

    for (std::size_t windowIndex = 0; windowIndex < windows.size(); ++windowIndex)
    {
        auto *window = windows[windowIndex];
        if (!window)
            return vigine::Result(vigine::Result::Code::Error,
                                  "Window component is unavailable");

        auto eventHandlers = deps.platformService->windowEventHandlers(entity, window);
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
            // resolve every tick; the lookup is cheap (registry hash +
            // dynamic_cast) and keeps the "no caches" rule visible.
            auto frameDeps = resolveDeps();

            bool resizedThisTick = false;

            if (_resizePending && frameDeps.renderSystem)
            {
                const auto now    = std::chrono::steady_clock::now();
                const bool paused = now - _lastResizeEvent >= std::chrono::milliseconds(80);
                if (paused)
                {
                    if (_pendingResizeWidth != _appliedResizeWidth ||
                        _pendingResizeHeight != _appliedResizeHeight)
                    {
                        const bool resized = frameDeps.renderSystem->resize(
                            _pendingResizeWidth, _pendingResizeHeight);
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

            if (frameDeps.textEditorSystem)
                frameDeps.textEditorSystem->onFrame();

            if (frameDeps.renderSystem && !resizedThisTick)
            {
                frameDeps.renderSystem->update();
#ifdef _WIN32
                if (auto *winApiWindow =
                        dynamic_cast<vigine::ecs::platform::WinAPIComponent *>(window))
                    winApiWindow->setRenderedVertexCount(
                        frameDeps.renderSystem->lastRenderedVertexCount());
#endif
            }
        });
        auto showResult = deps.platformService->showWindow(window);
        if (showResult.isError())
            return showResult;
        std::cout << "[RunWindowTask] Window " << (windowIndex + 1) << " closed, continuing"
                  << std::endl;
    }

    // Drop the expiration subscription explicitly so a late FSM
    // invalidation that arrives between this point and dtor cannot
    // dereference the cached service pointers below. The dtor would
    // do this anyway, but ordering keeps the contract obvious.
    _expirationSubscription.reset();

    // Ask the engine to stop the main pump so @c IEngine::run returns
    // to @c main and the process exits cleanly.
    deps.engine->shutdown();

    return vigine::Result();
}

void RunWindowTask::onMouseButtonDown(vigine::ecs::platform::MouseButton button, int x, int y)
{
    auto deps = resolveDeps();

    if (button == vigine::ecs::platform::MouseButton::Left)
    {
        vigine::Entity *picked = deps.renderSystem
                                     ? deps.renderSystem->pickFirstIntersectedEntity(x, y)
                                     : nullptr;

        _lastMouseRayX     = x;
        _lastMouseRayY     = y;
        _hasMouseRaySample = true;

        const bool pickedTextEditor =
            deps.textEditorSystem && deps.textEditorSystem->isEditorEntity(picked);

        // Freeze click marker first, then build ray from the same click point.
        updateMouseClickSphereVisualization(deps, x, y);
        updateMouseRayVisualization(deps, x, y);

        bool consumedByScrollbar = false;
        if (pickedTextEditor && deps.textEditorSystem)
            consumedByScrollbar = deps.textEditorSystem->onMouseButtonDown(x, y, picked);

        if (pickedTextEditor && deps.textEditorSystem && !consumedByScrollbar)
            deps.textEditorSystem->onEditorClick(x, y);

        // In Ctrl camera-unlock mode keep current focus unchanged.
        if (!_ctrlHeld)
        {
            // Restore normal behavior: clicked entity receives focus, including text
            // editor.
            setFocusedEntity(deps, picked);

            if (_focusedEntity)
            {
                _movementKeyMask = 0;
                if (deps.renderSystem)
                {
                    deps.renderSystem->setMoveForwardActive(false);
                    deps.renderSystem->setMoveBackwardActive(false);
                    deps.renderSystem->setMoveLeftActive(false);
                    deps.renderSystem->setMoveRightActive(false);
                    deps.renderSystem->setMoveUpActive(false);
                    deps.renderSystem->setMoveDownActive(false);
                    deps.renderSystem->setSprintActive(false);
                }
            }
        }
    }

    if (deps.renderSystem && button == vigine::ecs::platform::MouseButton::Right &&
        (!_focusedEntity || _ctrlHeld || _objectDragActive))
        deps.renderSystem->beginCameraDrag(x, y);

    std::cout << "[RunWindowTask::onMouseButtonDown] button=" << static_cast<int>(button)
              << ", x=" << x << ", y=" << y << std::endl;
    if (deps.signalEmitter)
    {
        static_cast<void>(deps.signalEmitter->emit(
            std::make_unique<MouseButtonDownPayload>(button, x, y)));
    }
}

void RunWindowTask::onMouseButtonUp(vigine::ecs::platform::MouseButton button, int x, int y)
{
    static_cast<void>(x);
    static_cast<void>(y);

    auto deps = resolveDeps();

    if (button == vigine::ecs::platform::MouseButton::Left && deps.textEditorSystem)
        deps.textEditorSystem->onMouseButtonUp();

    if (deps.renderSystem && button == vigine::ecs::platform::MouseButton::Right &&
        (!_focusedEntity || _ctrlHeld || _objectDragActive))
        deps.renderSystem->endCameraDrag();
}

void RunWindowTask::onMouseMove(int x, int y)
{
    _lastMouseRayX     = x;
    _lastMouseRayY     = y;
    _hasMouseRaySample = true;

    auto deps = resolveDeps();

    if (_objectDragActive)
        updateObjectDrag(deps, x, y);

    if (deps.textEditorSystem)
        deps.textEditorSystem->onMouseMove(x, y);

    if (deps.renderSystem && (!_focusedEntity || _ctrlHeld || _objectDragActive))
        deps.renderSystem->updateCameraDrag(x, y);
}

void RunWindowTask::onMouseWheel(int delta, int x, int y)
{
    auto deps = resolveDeps();

    // In object-drag mode: wheel adjusts object distance from camera.
    if (_objectDragActive)
    {
        const float wheelSteps = static_cast<float>(delta) / 120.0f;
        // Wheel forward -> move object forward (farther from camera), and vice
        // versa.
        const float factor      = std::pow(0.90f, -wheelSteps);
        _dragDistanceFromCamera = (std::max)(0.15f, _dragDistanceFromCamera * factor);
        // Allow Z movement so the object actually moves in depth, not just XY.
        updateObjectDrag(deps, x, y, /*suppressZDelta=*/false);
        return;
    }

    // When text editor is focused (and Ctrl not held): scroll text, not camera.
    if (_focusedEntity && !_ctrlHeld && deps.textEditorSystem)
    {
        deps.textEditorSystem->onMouseWheel(delta);
        return;
    }

    if (deps.renderSystem && (!_focusedEntity || _ctrlHeld))
        deps.renderSystem->zoomCamera(delta);
}

void RunWindowTask::onKeyDown(const vigine::ecs::platform::KeyEvent &event)
{
    auto deps = resolveDeps();

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
            static_cast<void>(beginObjectDrag(deps, _focusedEntity, mx, my));
        }
        return;
    }

    if (event.keyCode == kKeyRayToggle && !event.isRepeat)
    {
        _mouseRayVisible = !_mouseRayVisible;

        if (_mouseRayVisible)
        {
            if (_hasMouseRaySample)
                updateMouseRayVisualization(deps, _lastMouseRayX, _lastMouseRayY);
        } else if (ensureMouseRayEntity(deps) && deps.renderSystem)
        {
            deps.renderSystem->bindEntity(_mouseRayEntity);
            if (auto *rc = deps.renderSystem->boundRenderComponent())
            {
                auto transform = rc->getTransform();
                transform.setPosition({0.0f, -100.0f, 0.0f});
                transform.setScale({0.01f, 0.01f, 0.01f});
                rc->setTransform(transform);
            }
            deps.renderSystem->unbindEntity();
        }
    }

    if (event.keyCode == kKeyBillboardToggle && !event.isRepeat)
    {
        if (deps.renderSystem)
            deps.renderSystem->toggleBillboard();
    }

    if (!_focusedEntity || _ctrlHeld || _objectDragActive)
        updateCameraMovementKey(deps.renderSystem, event.keyCode, true);

    if (event.keyCode == kKeyEscape)
    {
        setFocusedEntity(deps, nullptr);
        return;
    }

    if (handleClipboardShortcut(deps.textEditorSystem, event))
        return;

    if (deps.textEditorSystem && isFocusedTextEditor(deps.textEditorSystem))
        deps.textEditorSystem->onKeyDown(event.keyCode);

    if (!event.isRepeat)
        std::cout << "[RunWindowTask::onKeyDown] keyCode=" << event.keyCode
                  << ", scanCode=" << event.scanCode << std::endl;
    if (deps.signalEmitter)
    {
        static_cast<void>(deps.signalEmitter->emit(std::make_unique<KeyDownPayload>(event)));
    }
}

void RunWindowTask::onKeyUp(const vigine::ecs::platform::KeyEvent &event)
{
    auto deps = resolveDeps();

    if (event.keyCode == kKeyControl || event.keyCode == kKeyLeftControl ||
        event.keyCode == kKeyRightControl)
        _ctrlHeld = false;

    if (!_focusedEntity || _ctrlHeld || _objectDragActive)
        updateCameraMovementKey(deps.renderSystem, event.keyCode, false);
}

void RunWindowTask::updateCameraMovementKey(vigine::ecs::graphics::RenderSystem *renderSystem,
                                            unsigned int                          keyCode,
                                            bool                                  pressed)
{
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
    auto deps = resolveDeps();
    if (deps.textEditorSystem && isFocusedTextEditor(deps.textEditorSystem))
        deps.textEditorSystem->onChar(event, _movementKeyMask);
}

bool RunWindowTask::handleClipboardShortcut(
    const std::shared_ptr<TextEditorSystem> &textEditorSystem,
    const vigine::ecs::platform::KeyEvent   &event)
{
    if (!textEditorSystem || !isFocusedTextEditor(textEditorSystem))
        return false;

    const bool ctrlPressed = (event.modifiers & vigine::ecs::platform::KeyModifierControl) != 0;
    if (!ctrlPressed)
        return false;

    if (event.keyCode == 'C' || event.keyCode == 'X')
    {
#ifdef _WIN32
        const std::wstring wide = wideFromUtf8(textEditorSystem->text());
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
            textEditorSystem->clearText();
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
                    textEditorSystem->insertUtf8(utf8FromWide(wide));
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

bool RunWindowTask::isFocusedTextEditor(
    const std::shared_ptr<TextEditorSystem> &textEditorSystem) const
{
    if (!_focusedEntity || !textEditorSystem)
        return false;

    return textEditorSystem->isEditorEntity(_focusedEntity);
}

void RunWindowTask::setFocusedEntity(const Deps &deps, vigine::Entity *entity)
{
    if (entity == _focusedEntity)
        return;

    // Any focus change exits move mode.
    if (_objectDragActive)
        endObjectDrag();

    if (_focusedEntity && deps.renderSystem && _hasFocusedOriginalScale)
    {
        deps.renderSystem->bindEntity(_focusedEntity);
        if (auto *rc = deps.renderSystem->boundRenderComponent())
        {
            auto transform = rc->getTransform();
            transform.setScale(_focusedOriginalScale);
            rc->setTransform(transform);
        }
        deps.renderSystem->unbindEntity();
    }

    _focusedEntity           = entity;
    _hasFocusedOriginalScale = false;

    // Editor entities receive focus for input but should not get the scale-up
    // visual effect.
    const bool isEditor = deps.textEditorSystem && deps.textEditorSystem->isEditorEntity(entity);

    if (deps.textEditorSystem)
        deps.textEditorSystem->setFocused(_focusedEntity != nullptr && isEditor);

    if (_focusedEntity && deps.renderSystem && !isEditor)
    {
        deps.renderSystem->bindEntity(_focusedEntity);
        if (auto *rc = deps.renderSystem->boundRenderComponent())
        {
            auto transform        = rc->getTransform();
            _focusedOriginalScale = transform.getScale();
            transform.setScale(_focusedOriginalScale * 1.08f);
            rc->setTransform(transform);
            _hasFocusedOriginalScale = true;
        }
        deps.renderSystem->unbindEntity();
    }
}

bool RunWindowTask::beginObjectDrag(const Deps &deps, vigine::Entity *entity, int x, int y)
{
    if (!entity || !deps.renderSystem)
        return false;

    deps.renderSystem->bindEntity(entity);
    auto *rc = deps.renderSystem->boundRenderComponent();
    if (!rc)
    {
        deps.renderSystem->unbindEntity();
        return false;
    }

    const auto transform = rc->getTransform();
    deps.renderSystem->unbindEntity();

    glm::vec3 rayOrigin(0.0f);
    glm::vec3 rayDirection(0.0f);
    if (!deps.renderSystem->screenPointToRayFromNearPlane(x, y, rayOrigin, rayDirection))
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
    _dragEditorGroup    = (deps.textEditorSystem && deps.textEditorSystem->isEditorEntity(entity));
    _dragEntity         = entity;
    _dragGrabOffset     = transform.getPosition() - hit;
    return true;
}

void RunWindowTask::updateObjectDrag(const Deps &deps, int x, int y, bool suppressZDelta)
{
    if (!_objectDragActive || !_dragEntity || !deps.renderSystem)
        return;

    glm::vec3 rayOrigin(0.0f);
    glm::vec3 rayDirection(0.0f);
    if (!deps.renderSystem->screenPointToRayFromNearPlane(x, y, rayOrigin, rayDirection))
        return;

    const float dirLen = glm::length(rayDirection);
    if (dirLen < 1e-6f)
        return;
    rayDirection           /= dirLen;

    const glm::vec3 hit     = rayOrigin + rayDirection * _dragDistanceFromCamera;
    const glm::vec3 newPos  = hit + _dragGrabOffset;

    deps.renderSystem->bindEntity(_dragEntity);
    auto *dragRc = deps.renderSystem->boundRenderComponent();
    if (!dragRc)
    {
        deps.renderSystem->unbindEntity();
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
    // This handles the case when TextEditEntity itself is the drag target.
    if (_dragEditorGroup)
        dragRc->translateGlyphVertices(delta);

    deps.renderSystem->unbindEntity();

    if (_dragEditorGroup && deps.entityManager)
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
            auto *e = deps.entityManager->getEntityByAlias(alias);
            if (!e || e == _dragEntity)
                continue;

            deps.renderSystem->bindEntity(e);
            if (auto *rc = deps.renderSystem->boundRenderComponent())
            {
                auto tr = rc->getTransform();
                tr.setPosition(tr.getPosition() + delta);
                rc->setTransform(tr);

                if (std::strcmp(alias, "TextEditEntity") == 0)
                    rc->translateGlyphVertices(delta);
            }
            deps.renderSystem->unbindEntity();
        }

        if (deps.textEditorSystem)
        {
            deps.textEditorSystem->offsetEditorFrame(delta.x, delta.y, delta.z);
            deps.textEditorSystem->refreshEditorLayout();
        }
        // Mark glyph dirty once per drag frame to upload translated vertices to GPU.
        deps.renderSystem->markGlyphDirty();
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

bool RunWindowTask::ensureMouseRayEntity(const Deps &deps)
{
    if (_mouseRayEntity)
        return true;

    if (!deps.entityManager || !deps.renderSystem)
        return false;

    auto *existing = deps.entityManager->getEntityByAlias("MouseRayEntity");
    _mouseRayEntity = existing ? static_cast<vigine::Entity *>(existing) : nullptr;
    if (!_mouseRayEntity)
    {
        _mouseRayEntity = deps.entityManager->createEntity();
        if (!_mouseRayEntity)
            return false;

        deps.entityManager->addAlias(_mouseRayEntity, "MouseRayEntity");
    }

    deps.renderSystem->createComponents(_mouseRayEntity);
    deps.renderSystem->bindEntity(_mouseRayEntity);
    auto *rc = deps.renderSystem->boundRenderComponent();
    if (!rc)
    {
        deps.renderSystem->unbindEntity();
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

    deps.renderSystem->unbindEntity();
    return true;
}

bool RunWindowTask::ensureMouseClickSphereEntity(const Deps &deps)
{
    if (_mouseClickSphereEntity)
        return true;

    if (!deps.entityManager || !deps.renderSystem)
        return false;

    auto *existing = deps.entityManager->getEntityByAlias("MouseClickSphereEntity");
    _mouseClickSphereEntity = existing ? static_cast<vigine::Entity *>(existing) : nullptr;
    if (!_mouseClickSphereEntity)
    {
        _mouseClickSphereEntity = deps.entityManager->createEntity();
        if (!_mouseClickSphereEntity)
            return false;

        deps.entityManager->addAlias(_mouseClickSphereEntity, "MouseClickSphereEntity");
    }

    deps.renderSystem->createComponents(_mouseClickSphereEntity);
    deps.renderSystem->bindEntity(_mouseClickSphereEntity);
    auto *rc = deps.renderSystem->boundRenderComponent();
    if (!rc)
    {
        deps.renderSystem->unbindEntity();
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

    deps.renderSystem->unbindEntity();
    return true;
}

void RunWindowTask::updateMouseRayVisualization(const Deps &deps, int x, int y)
{
    if (!deps.renderSystem)
        return;

    if (!ensureMouseRayEntity(deps))
        return;

    glm::vec3 clickRayOrigin(0.0f);
    glm::vec3 clickRayDirection(0.0f);
    if (!deps.renderSystem->screenPointToRayFromNearPlane(x, y, clickRayOrigin, clickRayDirection))
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

    deps.renderSystem->bindEntity(_mouseRayEntity);
    if (auto *rc = deps.renderSystem->boundRenderComponent())
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
    deps.renderSystem->unbindEntity();
}

void RunWindowTask::updateMouseClickSphereVisualization(const Deps &deps, int x, int y)
{
    if (!deps.renderSystem)
        return;

    if (!ensureMouseClickSphereEntity(deps))
        return;

    glm::vec3 clickRayOrigin(0.0f);
    glm::vec3 clickRayDirection(0.0f);
    if (!deps.renderSystem->screenPointToRayFromNearPlane(x, y, clickRayOrigin, clickRayDirection))
        return;

    constexpr float kStartOffset = 0.03f;
    const glm::vec3 sphereCenter = clickRayOrigin + clickRayDirection * kStartOffset;

    deps.renderSystem->bindEntity(_mouseClickSphereEntity);
    if (auto *rc = deps.renderSystem->boundRenderComponent())
    {
        auto transform = rc->getTransform();
        transform.setPosition(sphereCenter);
        transform.setScale({0.05f, 0.05f, 0.05f});
        rc->setTransform(transform);
    }
    deps.renderSystem->unbindEntity();
}

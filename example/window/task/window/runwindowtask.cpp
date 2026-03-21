#include "runwindowtask.h"

#include "vigine/ecs/entitymanager.h"
#include "vigine/ecs/render/rendersystem.h"
#include <vigine/context.h>
#include <vigine/property.h>
#include <vigine/service/graphicsservice.h>
#include <vigine/service/platformservice.h>

#include "../../handler/windoweventhandler.h"
#include "ecs/platform/windowcomponent.h"

#include <iostream>

namespace
{
constexpr unsigned int kKeyW          = 'W';
constexpr unsigned int kKeyA          = 'A';
constexpr unsigned int kKeyS          = 'S';
constexpr unsigned int kKeyD          = 'D';
constexpr unsigned int kKeyQ          = 'Q';
constexpr unsigned int kKeyE          = 'E';
constexpr unsigned int kKeyShift      = 0x10;
constexpr unsigned int kKeyLeftShift  = 0xA0;
constexpr unsigned int kKeyRightShift = 0xA1;
} // namespace

RunWindowTask::RunWindowTask() {}

void RunWindowTask::contextChanged()
{
    if (!context())
    {
        _platformService = nullptr;
        _graphicsService = nullptr;
        _renderSystem    = nullptr;
        return;
    }

    _platformService = dynamic_cast<vigine::platform::PlatformService *>(
        context()->service("Platform", vigine::Name("MainPlatform"), vigine::Property::Exist));

    _graphicsService = dynamic_cast<vigine::graphics::GraphicsService *>(
        context()->service("Graphics", vigine::Name("MainGraphics"), vigine::Property::Exist));
    if (_graphicsService)
        _renderSystem = _graphicsService->renderSystem();
}

// COPILOT_TODO: Гарантувати unbindEntity() на всіх ранніх виходах через RAII/guard, інакше
// PlatformService може залишитися прив'язаним після помилки.
vigine::Result RunWindowTask::execute()
{
    if (!_platformService)
        return vigine::Result(vigine::Result::Code::Error, "Platform service is unavailable");

    auto *entityManager = context()->entityManager();
    auto *entity        = entityManager->getEntityByAlias("MainWindow");
    if (!entity)
        return vigine::Result(vigine::Result::Code::Error, "MainWindow entity not found");

    _platformService->bindEntity(entity);
    {
        auto windows = _platformService->windowComponents();
        if (windows.empty())
            return vigine::Result(vigine::Result::Code::Error, "Window component is unavailable");

        for (std::size_t windowIndex = 0; windowIndex < windows.size(); ++windowIndex)
        {
            auto *window = windows[windowIndex];
            if (!window)
                return vigine::Result(vigine::Result::Code::Error,
                                      "Window component is unavailable");

            auto eventHandlers = _platformService->windowEventHandlers(window);
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
                    [this](vigine::platform::MouseButton button, int x, int y) {
                        std::cout << "[RunWindowTask::execute::lambda] button="
                                  << static_cast<int>(button) << ", x=" << x << ", y=" << y
                                  << std::endl;
                        onMouseButtonDown(button, x, y);
                    });
                windowEventHandler->setMouseButtonUpCallback(
                    [this](vigine::platform::MouseButton button, int x, int y) {
                        onMouseButtonUp(button, x, y);
                    });
                windowEventHandler->setMouseMoveCallback(
                    [this](int x, int y) { onMouseMove(x, y); });
                windowEventHandler->setMouseWheelCallback(
                    [this](int delta, int x, int y) { onMouseWheel(delta, x, y); });
                windowEventHandler->setKeyDownCallback(
                    [this](const vigine::platform::KeyEvent &event) {
                        std::cout << "[RunWindowTask::execute::lambda] keyCode=" << event.keyCode
                                  << ", scanCode=" << event.scanCode
                                  << ", modifiers=" << event.modifiers
                                  << ", repeatCount=" << event.repeatCount
                                  << ", isRepeat=" << event.isRepeat << std::endl;
                        onKeyDown(event);
                    });
                windowEventHandler->setKeyUpCallback(
                    [this](const vigine::platform::KeyEvent &event) { onKeyUp(event); });
                windowEventHandler->setWindowResizedCallback([this, window](int width, int height) {
                    onWindowResized(window, width, height);
                });
            }

            std::cout << "[RunWindowTask] Showing window " << (windowIndex + 1) << std::endl;
            window->setFrameCallback([this]() {
                bool resizedThisTick = false;

                if (_resizePending && _renderSystem)
                {
                    const auto now    = std::chrono::steady_clock::now();
                    const bool paused = now - _lastResizeEvent >= std::chrono::milliseconds(80);
                    if (paused)
                    {
                        if (_pendingResizeWidth != _appliedResizeWidth ||
                            _pendingResizeHeight != _appliedResizeHeight)
                        {
                            const bool resized =
                                _renderSystem->resize(_pendingResizeWidth, _pendingResizeHeight);
                            if (!resized)
                            {
                                std::cerr
                                    << "[RunWindowTask] Failed to recreate swapchain on resize: "
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

                if (_renderSystem && !resizedThisTick)
                    _renderSystem->update();
            });
            auto showResult = _platformService->showWindow(window);
            if (showResult.isError())
                return showResult;
            std::cout << "[RunWindowTask] Window " << (windowIndex + 1) << " closed, continuing"
                      << std::endl;
        }
    }
    _platformService->unbindEntity();

    return vigine::Result();
}

void RunWindowTask::onMouseButtonDown(vigine::platform::MouseButton button, int x, int y)
{
    if (_renderSystem && button == vigine::platform::MouseButton::Left)
        _renderSystem->beginCameraDrag(x, y);

    std::cout << "[RunWindowTask::onMouseButtonDown] button=" << static_cast<int>(button)
              << ", x=" << x << ", y=" << y << std::endl;
    emitMouseButtonDownSignal(button, x, y);
}

void RunWindowTask::onMouseButtonUp(vigine::platform::MouseButton button, int x, int y)
{
    static_cast<void>(x);
    static_cast<void>(y);

    if (_renderSystem && button == vigine::platform::MouseButton::Left)
        _renderSystem->endCameraDrag();
}

void RunWindowTask::onMouseMove(int x, int y)
{
    if (_renderSystem)
        _renderSystem->updateCameraDrag(x, y);
}

void RunWindowTask::onMouseWheel(int delta, int x, int y)
{
    static_cast<void>(x);
    static_cast<void>(y);

    if (_renderSystem)
        _renderSystem->zoomCamera(delta);
}

void RunWindowTask::onKeyDown(const vigine::platform::KeyEvent &event)
{
    updateCameraMovementKey(event.keyCode, true);

    std::cout << "[RunWindowTask::onKeyDown] keyCode=" << event.keyCode
              << ", scanCode=" << event.scanCode << std::endl;
    emitKeyDownSignal(event);
}

void RunWindowTask::onKeyUp(const vigine::platform::KeyEvent &event)
{
    updateCameraMovementKey(event.keyCode, false);
}

void RunWindowTask::updateCameraMovementKey(unsigned int keyCode, bool pressed)
{
    if (!_renderSystem)
        return;

    switch (keyCode)
    {
    case kKeyW:
        _renderSystem->setMoveForwardActive(pressed);
        break;
    case kKeyS:
        _renderSystem->setMoveBackwardActive(pressed);
        break;
    case kKeyA:
        _renderSystem->setMoveLeftActive(pressed);
        break;
    case kKeyD:
        _renderSystem->setMoveRightActive(pressed);
        break;
    case kKeyQ:
        _renderSystem->setMoveDownActive(pressed);
        break;
    case kKeyE:
        _renderSystem->setMoveUpActive(pressed);
        break;
    case kKeyShift:
    case kKeyLeftShift:
    case kKeyRightShift:
        _renderSystem->setSprintActive(pressed);
        break;
    default:
        break;
    }
}

void RunWindowTask::onWindowResized(vigine::platform::WindowComponent *window, int width,
                                    int height)
{
    if (!_renderSystem || !_platformService || !window)
        return;

    if (width <= 0 || height <= 0)
        return;

    _pendingResizeWindow = window;
    _pendingResizeWidth  = static_cast<uint32_t>(width);
    _pendingResizeHeight = static_cast<uint32_t>(height);
    _resizePending       = true;
    _lastResizeEvent     = std::chrono::steady_clock::now();
}

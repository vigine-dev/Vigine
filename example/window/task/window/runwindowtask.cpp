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
                windowEventHandler->setKeyDownCallback(
                    [this](const vigine::platform::KeyEvent &event) {
                        std::cout << "[RunWindowTask::execute::lambda] keyCode=" << event.keyCode
                                  << ", scanCode=" << event.scanCode
                                  << ", modifiers=" << event.modifiers
                                  << ", repeatCount=" << event.repeatCount
                                  << ", isRepeat=" << event.isRepeat << std::endl;
                        onKeyDown(event);
                    });
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
    std::cout << "[RunWindowTask::onMouseButtonDown] button=" << static_cast<int>(button)
              << ", x=" << x << ", y=" << y << std::endl;
    emitMouseButtonDownSignal(button, x, y);
}

void RunWindowTask::onKeyDown(const vigine::platform::KeyEvent &event)
{
    std::cout << "[RunWindowTask::onKeyDown] keyCode=" << event.keyCode
              << ", scanCode=" << event.scanCode << std::endl;
    emitKeyDownSignal(event);
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

#include "runwindowtask.h"

#include "vigine/ecs/entitymanager.h"
#include <vigine/context.h>
#include <vigine/property.h>
#include <vigine/service/platformservice.h>

#include "../../handler/windoweventhandler.h"

#include <iostream>

RunWindowTask::RunWindowTask() {}

void RunWindowTask::contextChanged()
{
    if (!context())
    {
        _platformService = nullptr;
        return;
    }

    _platformService = dynamic_cast<vigine::platform::PlatformService *>(
        context()->service("Platform", vigine::Name("MainPlatform"), vigine::Property::Exist));
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
            }

            std::cout << "[RunWindowTask] Showing window " << (windowIndex + 1) << std::endl;
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

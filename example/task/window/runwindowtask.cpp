#include "runwindowtask.h"

#include "vigine/ecs/entitymanager.h"
#include <vigine/context.h>
#include <vigine/property.h>
#include <vigine/service/platformservice.h>

#include "handler/windoweventhandler.h"

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
        auto *eventHandler = _platformService->windowEventHandler();
        if (!eventHandler)
            return vigine::Result(vigine::Result::Code::Error,
                                  "Window event handler is unavailable");

        auto *windowEventHandler = dynamic_cast<WindowEventHandler *>(eventHandler);
        if (!windowEventHandler)
            return vigine::Result(vigine::Result::Code::Error,
                                  "Window event handler has unsupported type");

        windowEventHandler->setMouseButtonDownCallback(
            [this](vigine::platform::MouseButton button, int x, int y) {
                std::cout << "[RunWindowTask::execute::lambda] button=" << static_cast<int>(button)
                          << ", x=" << x << ", y=" << y << std::endl;
                onMouseButtonDown(button, x, y);
            });
        windowEventHandler->setKeyDownCallback([this](const vigine::platform::KeyEvent &event) {
            std::cout << "[RunWindowTask::execute::lambda] keyCode=" << event.keyCode
                      << ", scanCode=" << event.scanCode << ", modifiers=" << event.modifiers
                      << ", repeatCount=" << event.repeatCount << ", isRepeat=" << event.isRepeat
                      << std::endl;
            onKeyDown(event);
        });
        _platformService->showWindow();
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
              << ", scanCode=" << event.scanCode << ", modifiers=" << event.modifiers
              << ", repeatCount=" << event.repeatCount << ", isRepeat=" << event.isRepeat
              << std::endl;
    emitKeyDownSignal(event);
}

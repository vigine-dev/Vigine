#include "initwindowtask.h"

#include "vigine/ecs/entitymanager.h"
#include <vigine/context.h>
#include <vigine/property.h>
#include <vigine/service/platformservice.h>

#include "../../handler/windoweventhandler.h"

InitWindowTask::InitWindowTask() {}

void InitWindowTask::contextChanged()
{
    if (!context())
    {
        _platformService = nullptr;

        return;
    }

    _platformService = dynamic_cast<vigine::platform::PlatformService *>(
        context()->service("Platform", vigine::Name("MainPlatform"), vigine::Property::New));
}

void InitWindowTask::createEventHandlers()
{
    _eventHandlers.clear();
    _eventHandlers.push_back(std::make_unique<WindowEventHandler>("Handler 1"));
    _eventHandlers.push_back(std::make_unique<WindowEventHandler>("Handler 2"));
}

vigine::Result InitWindowTask::execute()
{
    if (!_platformService)
        return vigine::Result(vigine::Result::Code::Error, "Platform service is unavailable");

    auto *entityManager = context()->entityManager();
    auto *entity        = entityManager->createEntity();

    createEventHandlers();

    _platformService->bindEntity(entity);
    {
        for (auto &handler : _eventHandlers)
        {
            auto *window = _platformService->createWindow();
            if (!window)
            {
                _platformService->unbindEntity();
                return vigine::Result(vigine::Result::Code::Error, "No window component created");
            }

            auto bindResult = _platformService->bindWindowEventHandler(window, handler.get());
            if (bindResult.isError())
            {
                _platformService->unbindEntity();
                return bindResult;
            }
        }
    }
    _platformService->unbindEntity();

    entityManager->addAlias(entity, "MainWindow");

    return vigine::Result();
}

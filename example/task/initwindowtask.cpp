#include "initwindowtask.h"

#include "vigine/ecs/entitymanager.h"
#include <vigine/context.h>
#include <vigine/property.h>
#include <vigine/service/platformservice.h>

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

vigine::Result InitWindowTask::execute()
{
    if (!_platformService)
        return vigine::Result(vigine::Result::Code::Error, "Platform service is unavailable");

    auto *entityManager = context()->entityManager();
    auto *entity        = entityManager->createEntity();

    _platformService->bindEntity(entity);
    {
        _platformService->createWindow();
        _platformService->showWindow();
    }
    _platformService->unbindEntity();

    entityManager->addAlias(entity, "MainWindow");

    return vigine::Result();
}

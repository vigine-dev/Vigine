#include "initwindowtask.h"

#include <vigine/api/ecs/ientitymanager.h>
#include <vigine/api/engine/iengine_token.h>
#include <vigine/api/service/wellknown.h>
#include <vigine/impl/ecs/entitymanager.h>
#include <vigine/impl/ecs/platform/platformservice.h>

#include "../../handler/windoweventhandler.h"

InitWindowTask::InitWindowTask() = default;

void InitWindowTask::createEventHandlers()
{
    _eventHandlers.clear();
    _eventHandlers.push_back(std::make_unique<WindowEventHandler>("MainWindowHandler"));
}

vigine::Result InitWindowTask::run()
{
    auto *token = apiToken();
    if (!token)
        return vigine::Result(vigine::Result::Code::Error, "Engine token is unavailable");

    auto entityManagerResult = token->entityManager();
    if (!entityManagerResult.ok())
        return vigine::Result(vigine::Result::Code::Error, "Entity manager is unavailable");
    auto *entityManager =
        dynamic_cast<vigine::EntityManager *>(&entityManagerResult.value());
    if (!entityManager)
        return vigine::Result(vigine::Result::Code::Error,
                              "Entity manager has unexpected type");

    auto platformResult = token->service(vigine::service::wellknown::platformService);
    if (!platformResult.ok())
        return vigine::Result(vigine::Result::Code::Error, "Platform service is unavailable");

    auto *platformService =
        dynamic_cast<vigine::ecs::platform::PlatformService *>(&platformResult.value());
    if (!platformService)
        return vigine::Result(vigine::Result::Code::Error,
                              "Platform service has unexpected type");

    auto *entity = entityManager->createEntity();

    createEventHandlers();

    auto *window = platformService->createWindow();
    if (!window)
        return vigine::Result(vigine::Result::Code::Error, "No window component created");

    auto bindResult =
        platformService->bindWindowEventHandler(entity, window, _eventHandlers[0].get());
    if (bindResult.isError())
        return bindResult;

    entityManager->addAlias(entity, "MainWindow");

    return vigine::Result();
}

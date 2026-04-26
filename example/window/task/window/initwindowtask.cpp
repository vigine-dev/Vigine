#include "initwindowtask.h"

#include <vigine/api/engine/iengine_token.h>
#include "vigine/impl/ecs/entitymanager.h"
#include <vigine/impl/ecs/platform/platformservice.h>

#include "../../handler/windoweventhandler.h"

InitWindowTask::InitWindowTask() = default;

void InitWindowTask::setEntityManager(vigine::EntityManager *entityManager) noexcept
{
    _entityManager = entityManager;
}

void InitWindowTask::setPlatformServiceId(vigine::service::ServiceId id) noexcept
{
    _platformServiceId = id;
}

void InitWindowTask::createEventHandlers()
{
    _eventHandlers.clear();
    _eventHandlers.push_back(std::make_unique<WindowEventHandler>("MainWindowHandler"));
}

vigine::Result InitWindowTask::run()
{
    if (!_entityManager)
        return vigine::Result(vigine::Result::Code::Error, "EntityManager is unavailable");

    auto *token = apiToken();
    if (!token)
        return vigine::Result(vigine::Result::Code::Error, "Engine token is unavailable");

    auto platformResult = token->service(_platformServiceId);
    if (!platformResult.ok())
        return vigine::Result(vigine::Result::Code::Error, "Platform service is unavailable");

    auto *platformService =
        dynamic_cast<vigine::ecs::platform::PlatformService *>(&platformResult.value());
    if (!platformService)
        return vigine::Result(vigine::Result::Code::Error,
                              "Platform service has unexpected type");

    auto *entity = _entityManager->createEntity();

    createEventHandlers();

    auto *window = platformService->createWindow();
    if (!window)
        return vigine::Result(vigine::Result::Code::Error, "No window component created");

    auto bindResult =
        platformService->bindWindowEventHandler(entity, window, _eventHandlers[0].get());
    if (bindResult.isError())
        return bindResult;

    _entityManager->addAlias(entity, "MainWindow");

    return vigine::Result();
}

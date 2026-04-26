#include "initvulkantask.h"

#include <vigine/api/engine/iengine_token.h>
#include <vigine/impl/ecs/entitymanager.h>
#include <vigine/impl/ecs/graphics/rendersystem.h>
#include <vigine/impl/ecs/graphics/graphicsservice.h>
#include <vigine/impl/ecs/platform/platformservice.h>

#include <iostream>

InitVulkanTask::InitVulkanTask() = default;

void InitVulkanTask::setEntityManager(vigine::EntityManager *entityManager) noexcept
{
    _entityManager = entityManager;
}

void InitVulkanTask::setPlatformServiceId(vigine::service::ServiceId id) noexcept
{
    _platformServiceId = id;
}

void InitVulkanTask::setGraphicsServiceId(vigine::service::ServiceId id) noexcept
{
    _graphicsServiceId = id;
}

vigine::Result InitVulkanTask::run()
{
    std::cout << "Initializing Vulkan API..." << std::endl;

    if (!_entityManager)
        return vigine::Result(vigine::Result::Code::Error, "EntityManager is unavailable");

    auto *token = api();
    if (!token)
        return vigine::Result(vigine::Result::Code::Error, "Engine token is unavailable");

    auto platformResult = token->service(_platformServiceId);
    auto graphicsResult = token->service(_graphicsServiceId);
    if (!platformResult.ok() || !graphicsResult.ok())
        return vigine::Result(vigine::Result::Code::Error,
                              "Render or Platform service not available");

    auto *platformService =
        dynamic_cast<vigine::ecs::platform::PlatformService *>(&platformResult.value());
    auto *graphicsService =
        dynamic_cast<vigine::ecs::graphics::GraphicsService *>(&graphicsResult.value());
    if (!platformService || !graphicsService)
        return vigine::Result(vigine::Result::Code::Error,
                              "Platform/Graphics service has unexpected type");

    auto *renderSystem = graphicsService->renderSystem();
    if (!renderSystem)
        return vigine::Result(vigine::Result::Code::Error, "Render system not available");

    auto *entity = _entityManager->getEntityByAlias("MainWindow");
    if (!entity)
        return vigine::Result(vigine::Result::Code::Error, "MainWindow entity not found");

    auto windows = platformService->windowComponents(entity);
    if (windows.empty())
        return vigine::Result(vigine::Result::Code::Error, "No window components available");

    void *nativeWindowHandle = platformService->nativeWindowHandle(windows.front());
    if (!nativeWindowHandle)
        return vigine::Result(vigine::Result::Code::Error, "Native window handle is unavailable");

    if (!renderSystem->initialize(nativeWindowHandle, 940, 660))
        return vigine::Result(vigine::Result::Code::Error,
                              "Failed to initialize render system");

    std::cout << "Vulkan API initialized successfully" << std::endl;
    return vigine::Result();
}

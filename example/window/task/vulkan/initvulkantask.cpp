#include "initvulkantask.h"

#include <vigine/context.h>
#include <vigine/impl/ecs/entitymanager.h>
#include <vigine/ecs/render/rendersystem.h>
#include <vigine/property.h>
#include <vigine/service/graphicsservice.h>
#include <vigine/service/platformservice.h>

#include <iostream>

InitVulkanTask::InitVulkanTask() {}

void InitVulkanTask::contextChanged()
{
    if (!context())
    {
        _renderSystem    = nullptr;
        _platformService = nullptr;
        return;
    }

    auto *graphicsService = dynamic_cast<vigine::graphics::GraphicsService *>(
        context()->service("Graphics", vigine::Name("MainGraphics"), vigine::Property::Exist));
    if (!graphicsService)
    {
        graphicsService = dynamic_cast<vigine::graphics::GraphicsService *>(
            context()->service("Graphics", vigine::Name("MainGraphics"), vigine::Property::New));
    }

    _platformService = dynamic_cast<vigine::platform::PlatformService *>(
        context()->service("Platform", vigine::Name("MainPlatform"), vigine::Property::Exist));
    if (!_platformService)
    {
        _platformService = dynamic_cast<vigine::platform::PlatformService *>(
            context()->service("Platform", vigine::Name("MainPlatform"), vigine::Property::New));
    }

    if (!graphicsService)
    {
        std::cerr << "Graphics service not available" << std::endl;
        return;
    }

    _renderSystem = graphicsService->renderSystem();
    if (!_renderSystem)
    {
        std::cerr << "Render system not available" << std::endl;
        return;
    }

    std::cout << "RenderSystem initialized successfully" << std::endl;
}

vigine::Result InitVulkanTask::execute()
{
    std::cout << "Initializing Vulkan API..." << std::endl;

    if (!_renderSystem || !_platformService)
    {
        return vigine::Result(vigine::Result::Code::Error,
                              "Render or Platform service not available");
    }

    auto *entity = context()->entityManager()->getEntityByAlias("MainWindow");
    if (!entity)
    {
        return vigine::Result(vigine::Result::Code::Error, "MainWindow entity not found");
    }

    _platformService->bindEntity(entity);
    auto windows = _platformService->windowComponents();
    if (windows.empty())
    {
        _platformService->unbindEntity();
        return vigine::Result(vigine::Result::Code::Error, "No window components available");
    }

    void *nativeWindowHandle = _platformService->nativeWindowHandle(windows.front());
    _platformService->unbindEntity();

    if (!nativeWindowHandle)
    {
        return vigine::Result(vigine::Result::Code::Error, "Native window handle is unavailable");
    }

    if (!_renderSystem->initialize(nativeWindowHandle, 940, 660))
    {
        return vigine::Result(vigine::Result::Code::Error,
                              "Failed to initialize render system");
    }

    std::cout << "Vulkan API initialized successfully" << std::endl;
    return vigine::Result();
}

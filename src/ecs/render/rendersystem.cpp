#include "vigine/ecs/render/rendersystem.h"

#include "vigine/ecs/entity.h"
#include "vigine/ecs/render/rendercomponent.h"
#include "vigine/ecs/render/vulkanapi.h"

#include <iostream>

using namespace vigine::graphics;

RenderSystem::RenderSystem(const SystemName &name)
    : AbstractSystem(name), _vulkanAPI(std::make_unique<VulkanAPI>()),
      _boundEntityComponent(nullptr)
{
    // Initialize Vulkan API
    if (!_vulkanAPI->initializeInstance())
    {
        std::cerr << "Failed to initialize Vulkan instance" << std::endl;
    }

    if (!_vulkanAPI->selectPhysicalDevice())
    {
        std::cerr << "Failed to select physical device" << std::endl;
    }

    if (!_vulkanAPI->createLogicalDevice())
    {
        std::cerr << "Failed to create logical device" << std::endl;
    }
}

RenderSystem::~RenderSystem()
{
    _entityComponents.clear();
    _vulkanAPI.reset();
}

bool RenderSystem::hasComponents(Entity *entity) const
{
    return _entityComponents.find(entity) != _entityComponents.end();
}

void RenderSystem::createComponents(Entity *entity)
{
    if (hasComponents(entity))
        return;

    auto renderComponent      = std::make_unique<RenderComponent>();
    _entityComponents[entity] = std::move(renderComponent);
}

void RenderSystem::destroyComponents(Entity *entity)
{
    auto it = _entityComponents.find(entity);
    if (it != _entityComponents.end())
    {
        _entityComponents.erase(it);
    }
}

vigine::SystemId RenderSystem::id() const { return "Render"; }

void RenderSystem::update()
{
    if (_vulkanAPI && _vulkanAPI->hasSwapchain())
        static_cast<void>(_vulkanAPI->drawFrame());

    for (auto &pair : _entityComponents)
    {
        if (pair.second)
        {
            pair.second->render();
        }
    }
}

bool RenderSystem::initializeWindowSurface(void *nativeWindowHandle, uint32_t width,
                                           uint32_t height)
{
    if (!_vulkanAPI || !nativeWindowHandle)
        return false;

    if (!_vulkanAPI->createSurface(nativeWindowHandle))
        return false;

    return _vulkanAPI->createSwapchain(width, height);
}

bool RenderSystem::resize(uint32_t width, uint32_t height)
{
    if (!_vulkanAPI)
        return false;

    return _vulkanAPI->recreateSwapchain(width, height);
}

void RenderSystem::beginCameraDrag(int x, int y)
{
    if (_vulkanAPI)
        _vulkanAPI->beginCameraDrag(x, y);
}

void RenderSystem::updateCameraDrag(int x, int y)
{
    if (_vulkanAPI)
        _vulkanAPI->updateCameraDrag(x, y);
}

void RenderSystem::endCameraDrag()
{
    if (_vulkanAPI)
        _vulkanAPI->endCameraDrag();
}

void RenderSystem::zoomCamera(int delta)
{
    if (_vulkanAPI)
        _vulkanAPI->zoomCamera(delta);
}

void RenderSystem::setMoveForwardActive(bool active)
{
    if (_vulkanAPI)
        _vulkanAPI->setMoveForwardActive(active);
}

void RenderSystem::setMoveBackwardActive(bool active)
{
    if (_vulkanAPI)
        _vulkanAPI->setMoveBackwardActive(active);
}

void RenderSystem::setMoveLeftActive(bool active)
{
    if (_vulkanAPI)
        _vulkanAPI->setMoveLeftActive(active);
}

void RenderSystem::setMoveRightActive(bool active)
{
    if (_vulkanAPI)
        _vulkanAPI->setMoveRightActive(active);
}

void RenderSystem::setMoveUpActive(bool active)
{
    if (_vulkanAPI)
        _vulkanAPI->setMoveUpActive(active);
}

void RenderSystem::setMoveDownActive(bool active)
{
    if (_vulkanAPI)
        _vulkanAPI->setMoveDownActive(active);
}

void RenderSystem::setSprintActive(bool active)
{
    if (_vulkanAPI)
        _vulkanAPI->setSprintActive(active);
}

void RenderSystem::entityBound()
{
    // Find the first render component and set it as bound
    if (!_entityComponents.empty())
    {
        _boundEntityComponent = _entityComponents.begin()->second.get();
    }
}

void RenderSystem::entityUnbound() { _boundEntityComponent = nullptr; }

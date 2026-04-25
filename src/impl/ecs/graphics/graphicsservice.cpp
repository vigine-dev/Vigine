#include "vigine/impl/ecs/graphics/graphicsservice.h"

#include "vigine/context.h"
#include "vigine/impl/ecs/entity.h"
#include "vigine/impl/ecs/graphics/rendercomponent.h"
#include "vigine/impl/ecs/graphics/rendersystem.h"
#include "vigine/impl/ecs/graphics/texturecomponent.h"
#include "vigine/property.h"

vigine::ecs::graphics::GraphicsService::GraphicsService(const Name &name) : AbstractService(name) {}

void vigine::ecs::graphics::GraphicsService::contextChanged()
{
    if (!context())
    {
        _renderSystem = nullptr;
        return;
    }

    // Try to get existing RenderSystem
    _renderSystem = dynamic_cast<RenderSystem *>(
        context()->system("Render", "MainRender", vigine::Property::Exist));

    if (_renderSystem)
        return;

    // Create RenderSystem if it doesn't exist
    _renderSystem = dynamic_cast<RenderSystem *>(
        context()->system("Render", "MainRender", vigine::Property::New));
}

bool vigine::ecs::graphics::GraphicsService::initializeRender(void *nativeWindowHandle,
                                                          uint32_t width, uint32_t height)
{
    if (!_renderSystem)
        return false;

    return _renderSystem->initialize(nativeWindowHandle, width, height);
}

void vigine::ecs::graphics::GraphicsService::entityBound()
{
    auto *entity = getBoundEntity();
    if (!_renderSystem || !entity)
        return;

    if (!_renderSystem->hasComponents(entity))
        _renderSystem->createComponents(entity);

    _renderSystem->bindEntity(entity);
}

void vigine::ecs::graphics::GraphicsService::entityUnbound()
{
    if (_renderSystem)
        _renderSystem->unbindEntity();
}

vigine::ecs::graphics::RenderComponent *vigine::ecs::graphics::GraphicsService::renderComponent() const
{
    if (!_renderSystem)
        return nullptr;

    return _renderSystem->boundRenderComponent();
}

vigine::ecs::graphics::TextureComponent *vigine::ecs::graphics::GraphicsService::textureComponent() const
{
    if (!_renderSystem)
        return nullptr;

    return _renderSystem->boundTextureComponent();
}

vigine::ServiceId vigine::ecs::graphics::GraphicsService::id() const { return "Graphics"; }

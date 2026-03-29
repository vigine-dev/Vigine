#include "vigine/service/graphicsservice.h"

#include "vigine/context.h"
#include "vigine/ecs/entity.h"
#include "vigine/ecs/render/rendercomponent.h"
#include "vigine/ecs/render/rendersystem.h"
#include "vigine/ecs/render/texturecomponent.h"
#include "vigine/property.h"

vigine::graphics::GraphicsService::GraphicsService(const Name &name) : AbstractService(name) {}

void vigine::graphics::GraphicsService::contextChanged()
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

void vigine::graphics::GraphicsService::entityBound()
{
    auto *entity = getBoundEntity();
    if (!_renderSystem || !entity)
        return;

    if (!_renderSystem->hasComponents(entity))
        _renderSystem->createComponents(entity);

    _renderSystem->bindEntity(entity);
}

void vigine::graphics::GraphicsService::entityUnbound()
{
    if (_renderSystem)
        _renderSystem->unbindEntity();
}

vigine::graphics::RenderComponent *vigine::graphics::GraphicsService::renderComponent() const
{
    if (!_renderSystem)
        return nullptr;

    return _renderSystem->boundRenderComponent();
}

vigine::graphics::TextureComponent *vigine::graphics::GraphicsService::textureComponent() const
{
    if (!_renderSystem)
        return nullptr;

    return _renderSystem->boundTextureComponent();
}

vigine::ServiceId vigine::graphics::GraphicsService::id() const { return "Graphics"; }

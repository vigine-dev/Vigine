#include "vigine/service/graphicsservice.h"

#include "vigine/context.h"
#include "vigine/ecs/render/rendersystem.h"
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
    if (_renderSystem)
        _renderSystem->bindEntity(getBoundEntity());
}

void vigine::graphics::GraphicsService::entityUnbound()
{
    if (_renderSystem)
        _renderSystem->unbindEntity();
}

vigine::ServiceId vigine::graphics::GraphicsService::id() const { return "Graphics"; }

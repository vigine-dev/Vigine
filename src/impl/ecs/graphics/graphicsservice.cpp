#include "vigine/impl/ecs/graphics/graphicsservice.h"

#include "vigine/api/context/icontext.h"
#include "vigine/impl/ecs/graphics/rendercomponent.h"
#include "vigine/impl/ecs/graphics/rendersystem.h"
#include "vigine/impl/ecs/graphics/texturecomponent.h"

vigine::ecs::graphics::GraphicsService::GraphicsService(const Name &name)
    : vigine::service::AbstractService()
    , _name{name}
{
}

const vigine::Name &vigine::ecs::graphics::GraphicsService::name() const noexcept { return _name; }

void vigine::ecs::graphics::GraphicsService::setRenderSystem(RenderSystem *system) noexcept
{
    _renderSystem = system;
}

vigine::Result vigine::ecs::graphics::GraphicsService::onInit(vigine::IContext &context)
{
    // Modern lifecycle: chain to the wrapper base so the
    // @c isInitialised flag flips to @c true. Render-system attachment
    // is performed through @ref setRenderSystem; @ref onInit itself
    // does not perform the lookup because @ref vigine::IContext does
    // not yet expose a system locator.
    return vigine::service::AbstractService::onInit(context);
}

vigine::Result vigine::ecs::graphics::GraphicsService::onShutdown(vigine::IContext &context)
{
    // Drop the non-owning render-system handle before chaining up.
    _renderSystem = nullptr;
    return vigine::service::AbstractService::onShutdown(context);
}

bool vigine::ecs::graphics::GraphicsService::initializeRender(void *nativeWindowHandle,
                                                              std::uint32_t width,
                                                              std::uint32_t height)
{
    if (!_renderSystem)
        return false;

    return _renderSystem->initialize(nativeWindowHandle, width, height);
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

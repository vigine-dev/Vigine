#include "vigine/impl/ecs/graphics/graphicsservice.h"

#include "vigine/api/context/icontext.h"
#include "vigine/impl/ecs/graphics/rendercomponent.h"
#include "vigine/impl/ecs/graphics/rendersystem.h"
#include "vigine/impl/ecs/graphics/texturecomponent.h"

#include <cassert>
#include <utility>

vigine::ecs::graphics::GraphicsService::GraphicsService(const Name &name,
                                                        std::unique_ptr<RenderSystem> renderSystem)
    : vigine::service::AbstractService()
    , _name{name}
    , _renderSystem{std::move(renderSystem)}
{
    // Construction pre-condition: the service owns a non-null
    // render system for its entire lifetime so every accessor below
    // can dereference @ref _renderSystem unconditionally. The default
    // registration path in @ref AbstractContext supplies a default
    // @c RenderSystem; applications that override the default through
    // @c IContext::registerService also pass a non-null one.
    assert(_renderSystem && "GraphicsService: renderSystem must be non-null");
}

vigine::ecs::graphics::GraphicsService::~GraphicsService() = default;

const vigine::Name &vigine::ecs::graphics::GraphicsService::name() const noexcept { return _name; }

vigine::ecs::graphics::RenderSystem *
vigine::ecs::graphics::GraphicsService::renderSystem() const noexcept
{
    return _renderSystem.get();
}

vigine::Result vigine::ecs::graphics::GraphicsService::onInit(vigine::IContext &context)
{
    // Modern lifecycle: chain to the wrapper base so the
    // @c isInitialised flag flips to @c true. The render system was
    // wired in at construction time and is owned by this service for
    // its entire lifetime; @ref onInit therefore has no per-call
    // attachment work to do.
    return vigine::service::AbstractService::onInit(context);
}

vigine::Result vigine::ecs::graphics::GraphicsService::onShutdown(vigine::IContext &context)
{
    // The owned @c RenderSystem is torn down alongside this service
    // via the private @c unique_ptr; @ref onShutdown only flips the
    // @c isInitialised flag back through the wrapper base.
    return vigine::service::AbstractService::onShutdown(context);
}

bool vigine::ecs::graphics::GraphicsService::initializeRender(void *nativeWindowHandle,
                                                              std::uint32_t width,
                                                              std::uint32_t height)
{
    return _renderSystem->initialize(nativeWindowHandle, width, height);
}

vigine::ecs::graphics::RenderComponent *vigine::ecs::graphics::GraphicsService::renderComponent() const
{
    return _renderSystem->boundRenderComponent();
}

vigine::ecs::graphics::TextureComponent *vigine::ecs::graphics::GraphicsService::textureComponent() const
{
    return _renderSystem->boundTextureComponent();
}

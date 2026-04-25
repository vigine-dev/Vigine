#pragma once

/**
 * @file graphicsservice.h
 * @brief Concrete service that wires graphics-side rendering
 *        (render system, render and texture components) into the
 *        engine service container.
 */

#include "vigine/api/service/abstractservice.h"
#include "vigine/api/service/serviceid.h"
#include "vigine/base/name.h"
#include "vigine/result.h"

#include <cstdint>
#include <memory>

namespace vigine
{
namespace ecs
{
namespace graphics
{
class RenderComponent;
class RenderSystem;
class TextureComponent;

/**
 * @brief Graphics service: owns a @c RenderSystem and exposes render
 *        initialisation plus access to render and texture components.
 *
 * Instantiated by the engine and registered against the modern
 * @ref vigine::context::AbstractContext through
 * @c registerService. @ref initializeRender boots the underlying
 * render backend against a native window handle and a surface size;
 * the accessors @ref renderComponent and @ref textureComponent expose
 * per-entity render data for other services / systems to consume.
 *
 * Wrapper base (post #330): the service derives from the modern
 * @ref vigine::service::AbstractService. The legacy
 * @ref vigine::AbstractService base is retired here. The render
 * system itself is wired in through @ref setRenderSystem because
 * @ref vigine::IContext does not yet expose a system locator —
 * adding one is a separate architect decision tracked outside this
 * leaf.
 */
class GraphicsService : public vigine::service::AbstractService
{
  public:
    explicit GraphicsService(const Name &name);
    ~GraphicsService() override = default;

    /**
     * @brief Returns the instance name supplied at construction.
     */
    [[nodiscard]] const Name &name() const noexcept;

    [[nodiscard]] RenderSystem *renderSystem() const noexcept { return _renderSystem; }

    /**
     * @brief Attaches the @c RenderSystem this service exposes.
     *
     * Replaces the legacy @c contextChanged path that pulled the
     * render system out of @c Context::system. Called by the engine
     * bootstrapper after the render system has been constructed and
     * registered on the ECS substrate. Passing @c nullptr detaches.
     */
    void setRenderSystem(RenderSystem *system) noexcept;

    [[nodiscard]] bool initializeRender(void *nativeWindowHandle, std::uint32_t width, std::uint32_t height);
    [[nodiscard]] RenderComponent *renderComponent() const;
    [[nodiscard]] TextureComponent *textureComponent() const;

    /**
     * @brief Modern lifecycle entry point.
     *
     * Chains to @ref vigine::service::AbstractService::onInit so the
     * @ref isInitialised flag flips to @c true.
     */
    [[nodiscard]] vigine::Result onInit(vigine::IContext &context) override;

    /**
     * @brief Modern teardown entry point.
     *
     * Drops the render-system observer pointer and chains to
     * @ref vigine::service::AbstractService::onShutdown.
     */
    [[nodiscard]] vigine::Result onShutdown(vigine::IContext &context) override;

  private:
    Name _name;
    RenderSystem *_renderSystem{nullptr};
};

using GraphicsServiceUPtr = std::unique_ptr<GraphicsService>;
using GraphicsServiceSPtr = std::shared_ptr<GraphicsService>;

} // namespace graphics
} // namespace ecs
} // namespace vigine

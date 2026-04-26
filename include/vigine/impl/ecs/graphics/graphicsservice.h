#pragma once

/**
 * @file graphicsservice.h
 * @brief Concrete service that wires graphics-side rendering
 *        (render system, render and texture components) into the
 *        engine service container.
 */

#include "vigine/api/service/abstractservice.h"
#include "vigine/api/service/serviceid.h"
#include "vigine/api/base/name.h"
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
 * Wrapper base: the service derives from
 * @ref vigine::service::AbstractService. Ownership of the underlying
 * @c RenderSystem is taken at construction time through a
 * @c std::unique_ptr — there is no post-construction setter. The
 * service tears the system down through its private member when the
 * service itself is destroyed.
 */
class GraphicsService : public vigine::service::AbstractService
{
  public:
    /**
     * @brief Constructs the service taking ownership of @p renderSystem.
     *
     * The @ref AbstractContext default registration creates the service
     * with a default @c RenderSystem internally; applications that
     * need a different concrete render system pass a custom
     * @c std::unique_ptr<RenderSystem> here and register the resulting
     * service through @c IContext::registerService(svc, knownId) which
     * replaces the default at the slot.
     */
    GraphicsService(const Name &name, std::unique_ptr<RenderSystem> renderSystem);
    ~GraphicsService() override;

    /**
     * @brief Returns the instance name supplied at construction.
     */
    [[nodiscard]] const Name &name() const noexcept;

    /**
     * @brief Returns the owned @c RenderSystem.
     *
     * The pointer is valid for the service's lifetime; the service
     * owns the system through a @c std::unique_ptr and the accessor
     * never returns @c nullptr because the constructor refuses a null
     * argument (asserts in Debug, treats null as a constructor
     * pre-condition violation in Release).
     */
    [[nodiscard]] RenderSystem *renderSystem() const noexcept;

    [[nodiscard]] bool initializeRender(void *nativeWindowHandle, std::uint32_t width, std::uint32_t height);

    /**
     * @brief Returns the render component bound to the underlying
     *        @ref RenderSystem's currently bound entity.
     *
     * The service does not bind entities itself; binding happens at
     * the @ref RenderSystem level (driven by ECS @c entityBound
     * callbacks). This accessor is a thin pass-through to
     * @c RenderSystem::boundRenderComponent and therefore returns
     * @c nullptr when the render system has no bound entity or the
     * bound entity has no @c RenderComponent registered. Callers
     * MUST null-check the return value; the accessor is intentionally
     * tolerant of unbound state because example code inspects bindings
     * opportunistically during input handling.
     */
    [[nodiscard]] RenderComponent *renderComponent() const;

    /**
     * @brief Returns the texture component bound to the underlying
     *        @ref RenderSystem's currently bound entity.
     *
     * Same null-state contract as @ref renderComponent: returns
     * @c nullptr when the render system has no bound entity or the
     * bound entity has no @c TextureComponent. Callers MUST null-check.
     */
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
     * The owned @c RenderSystem is torn down alongside this service
     * via the private @c unique_ptr; @ref onShutdown only chains to
     * @ref vigine::service::AbstractService::onShutdown so the
     * @ref isInitialised flag flips back to @c false.
     */
    [[nodiscard]] vigine::Result onShutdown(vigine::IContext &context) override;

  private:
    Name                          _name;
    std::unique_ptr<RenderSystem> _renderSystem;
};

using GraphicsServiceUPtr = std::unique_ptr<GraphicsService>;
using GraphicsServiceSPtr = std::shared_ptr<GraphicsService>;

} // namespace graphics
} // namespace ecs
} // namespace vigine

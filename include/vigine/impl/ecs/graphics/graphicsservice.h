#pragma once

/**
 * @file graphicsservice.h
 * @brief Concrete service that wires graphics-side rendering
 *        (render system, render and texture components) into the
 *        engine service container.
 */

#include "vigine/abstractservice.h"

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
 * Instantiated by the engine and registered against a bound entity.
 * @ref initializeRender boots the underlying render backend against a
 * native window handle and a surface size; the accessors
 * @ref renderComponent and @ref textureComponent expose the per-entity
 * render data for other services / systems to consume.
 */
class GraphicsService : public AbstractService
{
  public:
    GraphicsService(const Name &name);
    ~GraphicsService() override = default;

    [[nodiscard]] ServiceId id() const override;

    RenderSystem *renderSystem() const { return _renderSystem; }
    [[nodiscard]] bool initializeRender(void *nativeWindowHandle, uint32_t width, uint32_t height);
    RenderComponent *renderComponent() const;
    TextureComponent *textureComponent() const;

  protected:
    void contextChanged() override;

    void entityBound() override;
    void entityUnbound() override;

  private:
    RenderSystem *_renderSystem{nullptr};
};

using GraphicsServiceUPtr = std::unique_ptr<GraphicsService>;
using GraphicsServiceSPtr = std::shared_ptr<GraphicsService>;

} // namespace graphics
} // namespace ecs
} // namespace vigine

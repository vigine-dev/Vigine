#pragma once

#include "vigine/abstractservice.h"
#include "vigine/base/macros.h"

namespace vigine
{
namespace graphics
{
class RenderComponent;
class RenderSystem;
class TextureComponent;

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

BUILD_SMART_PTR(GraphicsService);

} // namespace graphics
} // namespace vigine

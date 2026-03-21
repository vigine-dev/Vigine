#pragma once

#include "vigine/abstractservice.h"
#include "vigine/base/macros.h"
#include "vigine/ecs/entity.h"
#include "vigine/result.h"

namespace vigine
{
namespace graphics
{
class RenderSystem;

class GraphicsService : public AbstractService
{
  public:
    GraphicsService(const Name &name);
    ~GraphicsService() override = default;

    [[nodiscard]] ServiceId id() const override;

    RenderSystem *renderSystem() const { return _renderSystem; }

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

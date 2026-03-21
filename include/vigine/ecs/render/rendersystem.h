#pragma once

#include "vigine/base/macros.h"
#include "vigine/ecs/abstractsystem.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace vigine
{
namespace graphics
{

class RenderComponent;
class VulkanAPI;

// TODO: create skeleton
class RenderSystem : public AbstractSystem
{
  public:
    RenderSystem(const SystemName &name);
    ~RenderSystem() override;

    [[nodiscard]] SystemId id() const override;

    // interface implementation
    [[nodiscard]] bool hasComponents(Entity *entity) const override;
    void createComponents(Entity *entity) override;
    void destroyComponents(Entity *entity) override;

    void update();
    [[nodiscard]] bool initializeWindowSurface(void *nativeWindowHandle, uint32_t width,
                                               uint32_t height);
    [[nodiscard]] bool resize(uint32_t width, uint32_t height);

  protected:
    virtual void entityBound();
    virtual void entityUnbound();

  private:
    std::unique_ptr<VulkanAPI> _vulkanAPI;
    std::unordered_map<Entity *, std::unique_ptr<RenderComponent>> _entityComponents;
    RenderComponent *_boundEntityComponent;
};

BUILD_SMART_PTR(RenderSystem);

} // namespace graphics
} // namespace vigine

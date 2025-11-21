#pragma once

#include "vigine/base/macros.h"
#include "vigine/ecs/abstractsystem.h"

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

    SystemId id() const override;

    // interface implementation
    bool hasComponents(Entity *entity) const override;
    void createComponents(Entity *entity) override;
    void destroyComponents(Entity *entity) override;

    void update();

  protected:
    virtual void entityBound();
    virtual void entityUnbound();

  private:
    // std::unordered_map<Entity *, std::unique_ptr<RenderComponent>> _entityComponents;
    // RenderComponent *_boundEntityComponent;
    std::unique_ptr<VulkanAPI> _vulkanAPI;
};

BUILD_SMART_PTR(RenderSystem);

} // namespace graphics
} // namespace vigine

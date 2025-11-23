#pragma once

#include "vigine/base/macros.h"
#include "vigine/ecs/abstractsystem.h"

#include <unordered_map>

namespace vigine
{
namespace platform
{
class WindowComponent;

class WindowSystem : public AbstractSystem
{
  public:
    WindowSystem(const SystemName &name);
    ~WindowSystem() override;

    SystemId id() const override;

    // interface implementation
    bool hasComponents(Entity *entity) const override;
    void createComponents(Entity *entity) override;
    void destroyComponents(Entity *entity) override;

  protected:
    virtual void entityBound();
    virtual void entityUnbound();

  private:
    std::unordered_map<Entity *, std::unique_ptr<WindowComponent>> _entityComponents;
    WindowComponent *_boundEntityComponent;
};

BUILD_SMART_PTR(WindowSystem);

} // namespace platform
} // namespace vigine

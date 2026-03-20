#pragma once

#include "vigine/entitybindinghost.h"

#include <memory>
#include <string>

namespace vigine
{
using SystemId   = std::string;
using SystemName = std::string;

class Entity;

class AbstractSystem : public EntityBindingHost
{
  public:
    ~AbstractSystem() override;

    [[nodiscard]] virtual SystemId id() const = 0;
    [[nodiscard]] SystemName name();

    [[nodiscard]] virtual bool hasComponents(Entity *entity) const = 0;
    virtual void createComponents(Entity *entity)                  = 0;
    virtual void destroyComponents(Entity *entity)                 = 0;

  protected:
    AbstractSystem(const SystemName &name);

  private:
    SystemName _name;
};

using AbstractSystemUPtr = std::unique_ptr<AbstractSystem>;

} // namespace vigine

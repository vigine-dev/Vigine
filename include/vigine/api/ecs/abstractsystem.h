#pragma once

/**
 * @file abstractsystem.h
 * @brief Legacy abstract base for ECS systems that create / destroy components.
 */

#include "vigine/api/ecs/isystem.h"
#include "vigine/entitybindinghost.h"

#include <memory>
#include <string>

namespace vigine
{
using SystemId   = std::string;
using SystemName = std::string;

class Entity;

/**
 * @brief Stateful abstract base for systems that attach components to
 *        a bound Entity.
 *
 * @ref AbstractSystem derives from the pure @ref vigine::ecs::ISystem
 * contract (which exposes only the @ref ISystem::id accessor and the
 * virtual destructor) and from @ref EntityBindingHost so exactly one
 * Entity is bound at a time. Concrete systems expose a stable kind via
 * @ref id, carry an instance @ref name, and implement create /
 * destroy / has-components against the bound Entity.
 */
class AbstractSystem : public vigine::ecs::ISystem, public EntityBindingHost
{
  public:
    ~AbstractSystem() override;

    [[nodiscard]] SystemId id() const override = 0;
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

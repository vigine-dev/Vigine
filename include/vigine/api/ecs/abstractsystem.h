#pragma once

/**
 * @file abstractsystem.h
 * @brief Legacy abstract base for ECS systems that create / destroy components.
 */

#include "vigine/api/ecs/isystem.h"

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
 * virtual destructor). Exactly one Entity may be bound at a time; the
 * Entity pointer is held by composition (private member) and reached
 * through @ref bindEntity / @ref unbindEntity / @ref getBoundEntity.
 * Concrete systems may override @ref entityBound / @ref entityUnbound
 * to react to binding changes. The previous EntityBindingHost mixin
 * has been deleted; this class now owns the binding state directly.
 */
class AbstractSystem : public vigine::ecs::ISystem
{
  public:
    ~AbstractSystem() override;

    [[nodiscard]] SystemId id() const override = 0;
    [[nodiscard]] SystemName name();

    [[nodiscard]] virtual bool hasComponents(Entity *entity) const = 0;
    virtual void createComponents(Entity *entity)                  = 0;
    virtual void destroyComponents(Entity *entity)                 = 0;

    void bindEntity(Entity *entity);
    void unbindEntity();
    [[nodiscard]] Entity *getBoundEntity() const;

  protected:
    AbstractSystem(const SystemName &name);

    virtual void entityBound();
    virtual void entityUnbound();

  private:
    SystemName _name;
    Entity *_entity{nullptr};
};

using AbstractSystemUPtr = std::unique_ptr<AbstractSystem>;

} // namespace vigine

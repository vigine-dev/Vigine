#pragma once

/**
 * @file abstractentity.h
 * @brief Legacy abstract base class for ECS entities.
 */

#include "vigine/api/ecs/ientity.h"

namespace vigine
{
/**
 * @brief Stateful abstract base for legacy ECS entity classes.
 *
 * @ref AbstractEntity derives from the pure @ref IEntity contract and
 * carries the (currently empty) state concrete entities share. The
 * base implements @ref IEntity::id with a default of @c 0; the
 * concrete @ref Entity in @c include/vigine/impl/ecs/entity.h overrides
 * the accessor when it is registered with the entity manager.
 *
 * The base carries no data members today; the door is open for the
 * follow-up leaf to fold shared entity state in (e.g. component
 * attachment list) without breaking source compatibility.
 */
class AbstractEntity : public IEntity
{
  public:
    ~AbstractEntity() override = default;

    [[nodiscard]] IEntity::Id id() const noexcept override { return _id; }

    AbstractEntity(const AbstractEntity &)            = delete;
    AbstractEntity &operator=(const AbstractEntity &) = delete;
    AbstractEntity(AbstractEntity &&)                 = delete;
    AbstractEntity &operator=(AbstractEntity &&)      = delete;

  protected:
    AbstractEntity() = default;

    /**
     * @brief Stamps the stable id reported by @ref IEntity::id.
     *
     * Called by the entity manager when the entity is registered;
     * concrete entities outside the manager should not call this.
     */
    void setId(IEntity::Id id) noexcept { _id = id; }

  private:
    IEntity::Id _id{0};
};

} // namespace vigine

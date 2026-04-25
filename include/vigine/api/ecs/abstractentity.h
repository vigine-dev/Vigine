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
 * owns the stable id slot every concrete entity reports. The base
 * stores @c _id (defaulting to @c 0 -- the unregistered sentinel) and
 * implements @ref IEntity::id directly; concrete subclasses do not
 * override the accessor. The concrete @ref Entity in
 * @c include/vigine/impl/ecs/entity.h stamps the slot through the
 * protected @ref setId hook from its @ref Entity(IEntity::Id)
 * constructor when the entity manager builds it.
 *
 * The id slot is the only data member today; the door is open for the
 * follow-up leaf to fold additional shared entity state in (e.g. a
 * component attachment list) without breaking source compatibility.
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

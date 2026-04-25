#pragma once

#include <cstdint>

namespace vigine
{
/**
 * @brief Pure-virtual root interface for legacy entity types.
 *
 * @ref IEntity is the minimal contract every legacy entity carries.
 * Concrete entity classes (currently @ref Entity in
 * @c include/vigine/impl/ecs/entity.h) derive from the abstract base
 * @ref AbstractEntity, which in turn implements this interface.
 *
 * The interface intentionally exposes only:
 *   - a virtual destructor (so callers can hold owning
 *     @c std::unique_ptr<IEntity>),
 *   - a stable @ref id accessor that the concrete subclasses populate
 *     when they are spawned by the entity manager.
 *
 * Lifetime: instances are owned by the entity manager. Callers receive
 * non-owning pointers / references; copying and moving are deleted on
 * the interface itself so a slice can never duplicate an entity.
 *
 * Thread-safety: the interface does not fix one. The concrete
 * @ref EntityManager serialises mutations under its own mutex; reads of
 * @ref id are stable for the entity's lifetime once it has been
 * registered.
 *
 * INV-1 compliance: no template parameters anywhere on the surface.
 * INV-10 compliance: the @c I prefix marks a pure-virtual interface
 * with no state and no non-virtual method bodies. The legacy
 * @c vigine::Entity concrete in @c include/vigine/impl/ecs/entity.h
 * satisfies this contract today; @ref id is the only new accessor and
 * the concrete already carries a stable identity through its address.
 */
class IEntity
{
  public:
    /**
     * @brief Stable identifier the entity carries for the duration of
     *        its registration with the entity manager.
     *
     * The id is stamped by the entity manager when the entity is
     * created and is never recycled while the entity is alive. A value
     * of @c 0 marks an entity that has not yet been registered.
     */
    using Id = std::uint64_t;

    virtual ~IEntity() = default;

    /**
     * @brief Returns the stable identifier the entity manager stamped
     *        on this entity at creation time.
     */
    [[nodiscard]] virtual Id id() const noexcept = 0;

    IEntity(const IEntity &)            = delete;
    IEntity &operator=(const IEntity &) = delete;
    IEntity(IEntity &&)                 = delete;
    IEntity &operator=(IEntity &&)      = delete;

  protected:
    IEntity() = default;
};

} // namespace vigine

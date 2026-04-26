#pragma once

#include <string>

namespace vigine
{
class IEntity;

/**
 * @brief Pure-virtual surface of the engine-wide entity manager.
 *
 * @ref IEntityManager is the contract @ref IContext exposes through
 * @c IContext::entityManager. The default implementation is built by
 * @ref AbstractContext during construction (concrete @c EntityManager
 * in @c include/vigine/impl/ecs/entitymanager.h) so tasks always observe
 * a live, working manager without anyone wiring it up explicitly.
 * Applications that need a different concrete implementation override
 * the slot through @c IContext::setEntityManager — the prior owner is
 * destroyed via the unique_ptr slot's RAII chain.
 *
 * Surface shape:
 *   - @ref createEntity stamps a fresh entity, owns it, and returns a
 *     non-owning pointer the caller may use until the entity is
 *     removed.
 *   - @ref removeEntity drops the entity and any alias bindings it
 *     carried.
 *   - @ref addAlias / @ref getEntityByAlias provide string-keyed
 *     lookups so callers do not have to thread @ref IEntity::Id
 *     handles through the wiring path.
 *
 * Ownership: every entity returned through @ref createEntity is owned
 * by the manager. Callers receive a non-owning raw pointer and never
 * delete it; @ref removeEntity is the one path that releases an entity.
 *
 * Thread-safety: implementations document their own policy; the
 * default concrete @c EntityManager carries no internal
 * synchronisation today, so external callers must serialise create /
 * remove / alias mutations themselves. Reads through
 * @ref getEntityByAlias are safe to interleave with other reads on
 * the same instance.
 *
 * INV-1 compliance: no template parameters on the surface. INV-10
 * compliance: the @c I prefix marks a pure-virtual interface with no
 * state and no non-virtual method bodies.
 */
class IEntityManager
{
  public:
    virtual ~IEntityManager() = default;

    /**
     * @brief Allocates a fresh entity, stamps it with a non-zero id,
     *        and stores it in the manager.
     *
     * Returns a non-owning pointer to the new entity. Ownership stays
     * with the manager; callers never delete the returned pointer.
     * Returns @c nullptr only if allocation itself fails (the default
     * concrete propagates @c std::bad_alloc through; embedders that
     * want a quieter contract override this method).
     */
    [[nodiscard]] virtual IEntity *createEntity() = 0;

    /**
     * @brief Releases @p entity from the manager and drops any alias
     *        bindings that named it.
     *
     * Idempotent: calling @ref removeEntity on a pointer the manager
     * does not own is a no-op. After the call returns, the pointer is
     * dangling; callers must drop their handle.
     */
    virtual void removeEntity(IEntity *entity) = 0;

    /**
     * @brief Binds @p alias to @p entity so future @ref getEntityByAlias
     *        lookups under that string return the same entity.
     *
     * Re-binding an alias to a different entity overwrites the prior
     * binding silently. Binding an alias to a null @p entity is
     * implementation-defined and the default concrete drops the
     * mapping rather than storing a null entry.
     */
    virtual void addAlias(IEntity *entity, const std::string &alias) = 0;

    /**
     * @brief Looks up the entity bound to @p alias.
     *
     * Returns @c nullptr when no entity is bound under @p alias.
     * Returned pointer is non-owning and stays valid until the entity
     * is removed.
     */
    [[nodiscard]] virtual IEntity *
        getEntityByAlias(const std::string &alias) const = 0;

    IEntityManager(const IEntityManager &)            = delete;
    IEntityManager &operator=(const IEntityManager &) = delete;
    IEntityManager(IEntityManager &&)                 = delete;
    IEntityManager &operator=(IEntityManager &&)      = delete;

  protected:
    IEntityManager() = default;
};

} // namespace vigine

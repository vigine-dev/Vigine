#pragma once

namespace vigine
{
/**
 * @brief Pure-virtual forward-declared stub for the entity-manager
 *        surface.
 *
 * @ref IEntityManager is a minimal stub whose only contract is a
 * virtual destructor. It exists so that @ref Engine::entityManager can
 * return a reference to a pure-virtual interface without requiring the
 * ECS domain to be finalised in this leaf. The finalised surface --
 * entity creation, removal, alias lookup -- lands in a later leaf that
 * refactors @c EntityManager onto this interface.
 *
 * Ownership: the stub is never instantiated directly. Concrete
 * @c EntityManager objects derive from it and are owned by the
 * @ref Engine as @c std::unique_ptr.
 */
class IEntityManager
{
  public:
    virtual ~IEntityManager() = default;

    IEntityManager(const IEntityManager &)            = delete;
    IEntityManager &operator=(const IEntityManager &) = delete;
    IEntityManager(IEntityManager &&)                 = delete;
    IEntityManager &operator=(IEntityManager &&)      = delete;

  protected:
    IEntityManager() = default;
};

} // namespace vigine

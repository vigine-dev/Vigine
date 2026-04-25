#pragma once

#include <memory>
#include <vector>

#include "vigine/api/ecs/componentkind.h"
#include "vigine/api/ecs/icomponent.h"
#include "vigine/result.h"

namespace vigine
{
/**
 * @brief Pure-virtual contract for the legacy component manager.
 *
 * @ref IComponentManager is the non-template surface the engine token
 * forward-declares in @c include/vigine/api/engine/iengine_token.h. It
 * replaces the type-template path the legacy
 * @ref vigine::ComponentManager carried before this leaf with virtual
 * dispatch keyed on @ref ComponentKind so the public API exports no
 * template parameters (INV-1).
 *
 * Surface shape:
 *   - @ref add takes ownership of an @ref IComponent and stores it
 *     under the kind the component reports through
 *     @ref IComponent::kind. The call returns
 *     @ref Result::Code::Error when the component is null or when its
 *     reported kind is @c ComponentKind::Unknown -- callers that have
 *     not yet been migrated onto the new contract therefore fail fast
 *     instead of silently piling up under a generic bucket.
 *   - @ref components returns a non-owning view over every component
 *     currently stored under @p kind. The order matches insertion
 *     order; equal-kind components keep their relative order.
 *   - @ref removeAllOfKind drops every component under @p kind and
 *     reports the count of components released. Idempotent: a second
 *     call returns @c 0 without side effects.
 *   - @ref clear drops every stored component. Used by the engine
 *     teardown path.
 *
 * Ownership: the manager takes unique ownership of every component it
 * accepts; the @ref components accessor returns non-owning pointers.
 * Callers that need an owning handle keep their own
 * @c std::shared_ptr at the call site.
 *
 * INV-1 compliance: no template parameters anywhere on the public
 * surface. INV-10 compliance: the @c I prefix marks a pure-virtual
 * interface with no state and no non-virtual method bodies.
 */
class IComponentManager
{
  public:
    virtual ~IComponentManager() = default;

    /**
     * @brief Takes ownership of @p component and stores it under the
     *        kind reported by @ref IComponent::kind.
     *
     * Returns @ref Result::Code::Error when @p component is null or
     * when its reported kind is @c ComponentKind::Unknown. On success
     * the manager retains the component until @ref removeAllOfKind or
     * @ref clear releases it.
     */
    [[nodiscard]] virtual Result add(std::unique_ptr<IComponent> component) = 0;

    /**
     * @brief Returns non-owning views of every component currently
     *        stored under @p kind.
     *
     * Returns an empty vector when no component matches.
     */
    [[nodiscard]] virtual std::vector<const IComponent *>
        components(ComponentKind kind) const = 0;

    /**
     * @brief Drops every component currently stored under @p kind.
     *
     * Returns the number of components released. Idempotent: a second
     * call with the same @p kind returns @c 0 without side effects.
     */
    virtual std::size_t removeAllOfKind(ComponentKind kind) noexcept = 0;

    /**
     * @brief Drops every component the manager currently stores.
     */
    virtual void clear() noexcept = 0;

    IComponentManager(const IComponentManager &)            = delete;
    IComponentManager &operator=(const IComponentManager &) = delete;
    IComponentManager(IComponentManager &&)                 = delete;
    IComponentManager &operator=(IComponentManager &&)      = delete;

  protected:
    IComponentManager() = default;
};

/**
 * @brief Empty intermediate base between @ref IComponentManager and
 *        the concrete @ref ComponentManager.
 *
 * @ref AbstractComponentManager is the level-4 hook in the wrapper
 * recipe applied to the legacy component manager. It deliberately
 * carries no state and provides no implementations of the pure-virtual
 * @ref IComponentManager methods today -- every method stays pure on
 * this layer and the concrete @ref ComponentManager closes the chain.
 * The class exists so the follow-up leaf can fold shared storage or
 * book-keeping (e.g. lifting the @c ComponentKind storage map) into
 * this layer without changing the inheritance hierarchy or the
 * @ref ComponentManager call sites.
 *
 * State carried here:
 *   - none today; the concrete @ref ComponentManager owns the storage
 *     map until the follow-up leaf migrates the substrate up into
 *     this base. Mirrors how @c AbstractEntityManager defers its
 *     storage to the concrete for the duration of the leaf.
 *
 * Strict encapsulation: per the project rule every data member that
 * eventually lands here will be @c private; derived closers will
 * reach the substrate through @c protected accessors. Today no
 * accessor is exposed because no state lives here.
 *
 * INV-1 compliance: no template parameters on the public surface.
 */
class AbstractComponentManager : public IComponentManager
{
  public:
    ~AbstractComponentManager() override = default;

    AbstractComponentManager(const AbstractComponentManager &)            = delete;
    AbstractComponentManager &operator=(const AbstractComponentManager &) = delete;
    AbstractComponentManager(AbstractComponentManager &&)                 = delete;
    AbstractComponentManager &operator=(AbstractComponentManager &&)      = delete;

  protected:
    AbstractComponentManager() = default;
};

} // namespace vigine

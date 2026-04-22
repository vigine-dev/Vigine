#pragma once

#include <memory>
#include <vector>

#include "vigine/result.h"
#include "vigine/service/iservice.h"
#include "vigine/service/serviceid.h"

namespace vigine::service
{
// Forward declaration only. The concrete ServiceRegistry type is a
// substrate-primitive specialisation defined under src/service and is
// never exposed in the public header tree — see INV-11, wrapper
// encapsulation.
class ServiceRegistry;

/**
 * @brief Stateful abstract base that every concrete service derives
 *        from.
 *
 * @ref AbstractService is level 4 of the wrapper recipe used by the
 * engine's Level-1 subsystem wrappers. It carries the state every
 * concrete service shares — the declared dependency list, the
 * initialised flag, and a private handle to the internal service
 * registry — and supplies default implementations of the
 * @ref IService lifecycle methods so that minimal concrete services
 * only need to override @ref IService::id (and optionally refine
 * @ref onInit / @ref onShutdown to add their own domain behaviour).
 *
 * The class carries state, so it follows the project's @c Abstract
 * naming convention rather than the @c I pure-virtual prefix. The base
 * is abstract only in the logical sense: its default @ref id returns
 * the sentinel so instantiating it directly would yield a service that
 * cannot be looked up by handle. Concrete services seal the chain
 * (see @c src/service/defaultservice.{h,cpp} for the minimal closer
 * used by the factory in this leaf).
 *
 * Composition, not inheritance:
 *   - @ref AbstractService HAS-A private @c std::unique_ptr<ServiceRegistry>.
 *     It does @b not inherit from the substrate primitive at the
 *     wrapper level. The internal registry is the only place where
 *     substrate primitives enter the service stack, and it lives
 *     strictly under @c src/service. This keeps the public header tree
 *     free of substrate types (@ref INV-11) and makes the "a service
 *     IS NOT a substrate primitive" relationship explicit.
 *
 * Strict encapsulation:
 *   - All data members are @c private. Derived services reach state
 *     through @c protected accessors; there is no @c mark / @c markNot
 *     pair — the single setter takes the desired boolean value.
 *
 * Thread-safety: the base inherits the registry's thread-safety
 * policy (reader-writer mutex on the substrate primitive). Callers may
 * query @ref isInitialised concurrently with a lifecycle transition;
 * transitions are intended to run on the engine main thread so readers
 * observe the flag consistently.
 */
class AbstractService : public IService
{
  public:
    ~AbstractService() override;

    // ------ IService ------

    [[nodiscard]] ServiceId id() const noexcept override;
    [[nodiscard]] Result    onInit(IContext &context) override;
    [[nodiscard]] Result    onShutdown(IContext &context) override;
    [[nodiscard]] std::vector<std::shared_ptr<IService>>
                            dependencies() const override;
    [[nodiscard]] bool      isInitialised() const noexcept override;

    AbstractService(const AbstractService &)            = delete;
    AbstractService &operator=(const AbstractService &) = delete;
    AbstractService(AbstractService &&)                 = delete;
    AbstractService &operator=(AbstractService &&)      = delete;

  protected:
    AbstractService();

    /**
     * @brief Flips the initialised flag.
     *
     * The default @ref onInit calls @c setInitialised(true) on success
     * and the default @ref onShutdown calls @c setInitialised(false).
     * Derived services that override @ref onInit / @ref onShutdown for
     * domain-specific work chain up to the base implementation so the
     * flag tracks reality.
     *
     * Single setter with a @c bool parameter; no @c markInitialised /
     * @c markNotInitialised pair.
     */
    void setInitialised(bool value) noexcept;

    /**
     * @brief Appends a dependency to this service's declared list.
     *
     * Called by concrete constructors (or by the registration code
     * before @ref onInit runs) to wire cross-service edges that the
     * container's topological sort consumes. Adding a duplicate entry
     * is allowed but wasteful; the container treats duplicates as a
     * single edge.
     */
    void addDependency(std::shared_ptr<IService> dependency);

    /**
     * @brief Overwrites the stored @ref ServiceId for this service.
     *
     * The container stamps the id during registration; concrete
     * services do not call the setter themselves. It is exposed as
     * @c protected so that test fixtures can construct a service in
     * isolation (without going through a container) and still produce
     * a non-sentinel id for assertion purposes.
     */
    void setId(ServiceId id) noexcept;

  private:
    /**
     * @brief Owns the internal service registry.
     *
     * The registry is a substrate-primitive specialisation defined
     * under @c src/service; forward-declaring it here keeps the
     * substrate out of the public header tree. Held through a
     * @c std::unique_ptr so the registry's full definition does not
     * have to leak through this header.
     */
    std::unique_ptr<ServiceRegistry> _registry;

    /// Declared dependencies on other services. Cross-service
    /// ownership edges held as @c std::shared_ptr so a dependency
    /// target outlives every dependent.
    std::vector<std::shared_ptr<IService>> _dependencies;

    /// Stable identifier assigned by the container during registration.
    ServiceId _id{};

    /// Lifecycle flag: @c true between a successful @ref onInit and
    /// the matching @ref onShutdown.
    bool _initialised{false};
};

} // namespace vigine::service

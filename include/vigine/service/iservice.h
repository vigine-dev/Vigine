#pragma once

#include <memory>
#include <vector>

#include "vigine/result.h"
#include "vigine/service/serviceid.h"

namespace vigine
{
class IContext;
} // namespace vigine

namespace vigine::service
{
/**
 * @brief Pure-virtual Level-1 wrapper surface for an engine service.
 *
 * A service is an atomic container of capability — graphics, platform,
 * network, database, timer — that the engine boots in topological
 * order during start-up and tears down in reverse order on shutdown.
 * @ref IService is the user-facing contract every concrete service
 * implementation fulfils. It carries no state and no substrate of its
 * own; its role is to declare the shape the container layer and every
 * service implementation agree on.
 *
 * The public API is deliberately restricted to domain types
 * (@ref ServiceId, @ref Result, @ref vigine::IContext) per INV-11.
 * Substrate primitive types do not appear here; the stateful base
 * @ref AbstractService keeps its internal service registry as an
 * opaque member composed through a private @c std::unique_ptr so the
 * substrate is never leaked to the wrapper's public surface.
 *
 * Ownership and lifetime:
 *   - Concrete services are handed to the engine as
 *     @c std::unique_ptr<IService>. The engine takes ownership and
 *     manages the service for the lifetime of the surrounding
 *     container; callers hold raw references while the container is
 *     alive.
 *   - Lifecycle entry points (@ref onInit, @ref onShutdown) take a
 *     reference to @ref vigine::IContext so concrete implementations
 *     can reach sibling services, the system bus, the thread manager,
 *     and so on through a single injection point. The context outlives
 *     every service it visits.
 *
 * Thread-safety: the contract does not fix one. Concrete services
 * document their own policy; the typical shape is that @ref onInit and
 * @ref onShutdown run on the engine main thread while any downstream
 * domain methods are documented per service.
 *
 * INV-1 compliance: the surface uses no template parameters. INV-10
 * compliance: the name carries the @c I prefix to mark a pure-virtual
 * interface. INV-11 compliance: the surface exposes only service-
 * domain handles; the graph substrate stays hidden behind the
 * @ref AbstractService composition layer.
 */
class IService
{
  public:
    virtual ~IService() = default;

    /**
     * @brief Returns the registry-assigned identifier of this service.
     *
     * The id is stamped when the service is registered in the container;
     * before registration the default-constructed sentinel is reported.
     * Generational ids let callers hold a value handle without risking
     * a dangling reference after the underlying slot recycles.
     */
    [[nodiscard]] virtual ServiceId id() const noexcept = 0;

    /**
     * @brief Runs the service's start-up step.
     *
     * The container calls @ref onInit exactly once per successful
     * registration, after every dependency declared by
     * @ref dependencies has itself initialised. The @p context
     * reference is the engine-level aggregator; concrete services read
     * their dependencies through it. A success result moves the
     * service to the initialised state; any error result aborts the
     * init chain and triggers a reverse-topological rollback of the
     * services that succeeded before it.
     */
    [[nodiscard]] virtual Result onInit(IContext &context) = 0;

    /**
     * @brief Runs the service's teardown step.
     *
     * The container calls @ref onShutdown in reverse-topological
     * order during engine teardown, or during rollback of a failed
     * init chain. The call is best-effort: implementations release
     * their resources and return an error @ref Result if they cannot,
     * but the container continues walking the remaining services and
     * aggregates the errors into a single final report.
     */
    [[nodiscard]] virtual Result onShutdown(IContext &context) = 0;

    /**
     * @brief Reports the other services this one depends on.
     *
     * Every handle returned here is treated as a @ref DependencyKind
     * @c Required edge by the default topological sort. Services that
     * need @c Optional or @c RuntimeOnly semantics expose that through
     * their own constructors and attach the kind inside the wrapper
     * implementation before the DAG is built. Returning an empty
     * vector marks the service as a root of the init DAG.
     *
     * The returned vector carries @c std::shared_ptr<IService> handles
     * because dependencies are cross-service ownership edges; the
     * target service must outlive every dependent. Handles are copies
     * of the container's internal pointers, so the caller may inspect
     * them without synchronising with the container.
     */
    [[nodiscard]] virtual std::vector<std::shared_ptr<IService>>
        dependencies() const = 0;

    /**
     * @brief Returns @c true iff @ref onInit has been called and
     *        returned success without a matching @ref onShutdown.
     *
     * The flag is an observability hook; the container is the
     * authoritative tracker of init state. Concrete services flip the
     * flag through the protected setter on @ref AbstractService; they
     * never mutate private state directly.
     */
    [[nodiscard]] virtual bool isInitialised() const noexcept = 0;

    IService(const IService &)            = delete;
    IService &operator=(const IService &) = delete;
    IService(IService &&)                 = delete;
    IService &operator=(IService &&)      = delete;

  protected:
    IService() = default;
};

} // namespace vigine::service

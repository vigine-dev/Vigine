#pragma once

#include <memory>

#include "vigine/messaging/busconfig.h"
#include "vigine/messaging/busid.h"
#include "vigine/result.h"
#include "vigine/service/serviceid.h"

namespace vigine::ecs
{
class IECS;
} // namespace vigine::ecs

namespace vigine::messaging
{
class IMessageBus;
} // namespace vigine::messaging

namespace vigine::service
{
class IService;
} // namespace vigine::service

namespace vigine::statemachine
{
class IStateMachine;
} // namespace vigine::statemachine

namespace vigine::taskflow
{
class ITaskFlow;
} // namespace vigine::taskflow

namespace vigine::threading
{
class IThreadManager;
} // namespace vigine::threading

namespace vigine
{
/**
 * @brief Pure-virtual aggregator / service locator of every engine-wide
 *        resource.
 *
 * @ref IContext is the single DI surface every service receives through
 * @c IService::onInit. It owns no resources itself; the stateful base
 * @ref AbstractContext holds the actual @c std::unique_ptr handles and
 * supplies the accessors on top of them. The concrete
 * @c DefaultContext (see @c src/context) seals the inheritance chain
 * and is handed out by @ref createContext.
 *
 * Surface shape (UD-9):
 *   - One accessor per Level-1 wrapper (@c systemBus, @c ecs,
 *     @c stateMachine, @c taskFlow, @c threadManager).
 *   - One service locator (@c service, @c registerService).
 *   - One user-bus factory (@c createMessageBus) and an id-keyed
 *     lookup (@c messageBus).
 *   - A freeze-after-run boundary (@c freeze, @c isFrozen) that blocks
 *     topology mutation once @c Engine::run starts the main loop. All
 *     mutating calls (createMessageBus, registerService) return
 *     @ref Result::Code::TopologyFrozen after @ref freeze is called.
 *     Read-only accessors stay available.
 *
 * Strict construction order (AD-5 C8, encoded by @ref AbstractContext):
 *   1. @ref threading::IThreadManager (created first).
 *   2. system @ref messaging::IMessageBus (needs the thread manager).
 *   3. Level-1 wrappers (ECS, state machine, task flow).
 *   4. services registered through @ref registerService.
 *
 * Destruction is the reverse of construction and is enforced by member
 * declaration order in @ref AbstractContext; see that class for the
 * exact layout.
 *
 * Ownership:
 *   - Wrappers are held as @c std::unique_ptr on the concrete
 *     aggregator; their lifetime equals the context lifetime.
 *   - Buses and services are @c std::shared_ptr; the registry shares
 *     ownership with whichever caller kept a handle.
 *   - Callers never delete a resource obtained through an accessor;
 *     the context owns them.
 *
 * Thread-safety: read-only accessors are safe from any thread at any
 * time; mutating calls (createMessageBus, registerService) take the
 * context's internal mutex. Freeze is atomic with respect to
 * in-progress mutators: after @ref freeze returns, no mutator call
 * completes successfully.
 *
 * INV-1 compliance: no template parameters on the interface or any of
 * its methods. INV-10 compliance: the I-prefix marks a pure-virtual
 * interface with no state and no non-virtual method bodies.
 * INV-11 compliance: the public API exposes only wrapper interfaces
 * and POD handles; no graph primitive types
 * (@c NodeId, @c EdgeId, @c IGraph, ...) leak across the boundary.
 */
class IContext
{
  public:
    virtual ~IContext() = default;

    // ------ Messaging ------

    /**
     * @brief Returns the engine-wide system bus.
     *
     * The system bus is created by the factory in the second step of
     * the construction chain and lives until the aggregator is
     * destroyed. Callers receive a reference; the bus is owned by the
     * context through a @c std::shared_ptr stored on the concrete base.
     */
    [[nodiscard]] virtual messaging::IMessageBus &systemBus() = 0;

    /**
     * @brief Creates a new user bus and registers it on the context.
     *
     * Returns a @c std::shared_ptr so callers may keep the bus alive
     * independently of the context's registry; the context retains its
     * own reference in the registry so the bus survives for the
     * duration of the context's lifetime even when callers drop their
     * copy. Returns @c nullptr when the context is frozen; callers
     * detect the condition through @ref isFrozen or, for a richer
     * signal, through @ref registerService which uses an explicit
     * @ref Result::Code::TopologyFrozen.
     */
    [[nodiscard]] virtual std::shared_ptr<messaging::IMessageBus>
        createMessageBus(const messaging::BusConfig &config) = 0;

    /**
     * @brief Looks up a bus registered on this context by @p id.
     *
     * Returns @c nullptr when @p id is invalid, when the id is unknown
     * to the registry, or when the registered bus has been released.
     * The call does not take the freeze guard; lookups stay available
     * before and after freeze.
     */
    [[nodiscard]] virtual std::shared_ptr<messaging::IMessageBus>
        messageBus(messaging::BusId id) const = 0;

    // ------ Level-1 wrappers ------

    /**
     * @brief Returns the engine-wide ECS.
     *
     * The ECS is owned by the context through a @c std::unique_ptr;
     * callers receive a reference and never delete the returned object.
     */
    [[nodiscard]] virtual ecs::IECS &ecs() = 0;

    /**
     * @brief Returns the engine-wide state machine.
     */
    [[nodiscard]] virtual statemachine::IStateMachine &stateMachine() = 0;

    /**
     * @brief Returns the engine-wide task flow.
     */
    [[nodiscard]] virtual taskflow::ITaskFlow &taskFlow() = 0;

    // ------ Threading ------

    /**
     * @brief Returns the engine-wide thread manager.
     *
     * The thread manager is the first member the aggregator builds and
     * the last it destroys; every other wrapper depends on it, so the
     * construction and destruction orders in @ref AbstractContext are
     * strict.
     */
    [[nodiscard]] virtual threading::IThreadManager &threadManager() = 0;

    // ------ Service registry ------

    /**
     * @brief Looks up a service by @p id in the registry.
     *
     * Returns @c nullptr when @p id is the invalid sentinel, when no
     * service is registered under the id, or when the registered slot
     * has been recycled (stale generation). Callers that want a
     * non-null handle keep a @c std::shared_ptr alive themselves;
     * @ref service returns copies of the registry's entries so the
     * caller's reference count is independent of the registry's.
     */
    [[nodiscard]] virtual std::shared_ptr<service::IService>
        service(service::ServiceId id) const = 0;

    /**
     * @brief Registers @p service on the context.
     *
     * The context stamps the service with a fresh @ref service::ServiceId
     * and stores it in its registry. Callers obtain the stamped id
     * through @c service->id() after the call returns success. Returns
     * @ref Result::Code::TopologyFrozen when the context has been
     * frozen; returns @ref Result::Code::Error when @p service is
     * @c nullptr. The service is held by @c std::shared_ptr inside the
     * registry; callers may keep or drop their own handle.
     */
    [[nodiscard]] virtual Result
        registerService(std::shared_ptr<service::IService> service) = 0;

    // ------ Lifecycle boundary ------

    /**
     * @brief Freezes the topology so subsequent mutators report
     *        @ref Result::Code::TopologyFrozen.
     *
     * Called by the engine immediately before the main loop starts.
     * Idempotent: a second call is a no-op. After @ref freeze returns,
     * every @ref createMessageBus / @ref registerService call fails
     * fast; accessors remain available.
     */
    virtual void freeze() noexcept = 0;

    /**
     * @brief Reports whether the topology has been frozen.
     *
     * Safe to call from any thread at any time. A @c true return
     * guarantees that no further mutator call will succeed until the
     * context is destroyed.
     */
    [[nodiscard]] virtual bool isFrozen() const noexcept = 0;

    IContext(const IContext &)            = delete;
    IContext &operator=(const IContext &) = delete;
    IContext(IContext &&)                 = delete;
    IContext &operator=(IContext &&)      = delete;

  protected:
    IContext() = default;
};

} // namespace vigine

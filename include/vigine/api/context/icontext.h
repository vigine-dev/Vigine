#pragma once

#include <memory>

#include "vigine/api/messaging/busconfig.h"
#include "vigine/api/messaging/busid.h"
#include "vigine/result.h"
#include "vigine/api/service/serviceid.h"
#include "vigine/api/statemachine/stateid.h"

namespace vigine
{
class IEntityManager;
} // namespace vigine

namespace vigine::ecs
{
class IECS;
} // namespace vigine::ecs

namespace vigine::engine
{
class IEngine;
class IEngineToken;
} // namespace vigine::engine

namespace vigine::messaging
{
class IMessageBus;
class ISignalEmitter;
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

namespace vigine::core::threading
{
class IThreadManager;
} // namespace vigine::core::threading

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
 * @c Context (see @c include/vigine/impl/context) seals the inheritance
 * chain and is handed out by @ref createContext.
 *
 * Surface shape (UD-9):
 *   - One accessor per Level-1 wrapper (@c systemBus, @c ecs,
 *     @c stateMachine, @c taskFlow, @c threadManager).
 *   - One service locator (@c service, @c registerService).
 *   - One user-bus factory (@c createMessageBus) and an id-keyed
 *     lookup (@c messageBus).
 *   - One engine-token factory (@c makeEngineToken) that mints
 *     @ref vigine::engine::IEngineToken handles bound to a state. The
 *     token is the state-scoped DI surface tasks receive at
 *     @c onEnter; see the gating split below.
 *   - A freeze-after-run boundary (@c freeze, @c isFrozen) that blocks
 *     topology mutation once @c Engine::run starts the main loop.
 *     After @ref freeze is called, @c createMessageBus returns
 *     @c nullptr and @c registerService returns
 *     @ref Result::Code::TopologyFrozen. Read-only accessors stay
 *     available.
 *
 * Gating split (R-StateScope hybrid policy, mirrored on
 * @ref vigine::engine::IEngineToken):
 *   - @b Domain accessors -- @ref service, @ref ecs (and the
 *     follow-up @c entityManager / @c components / system-by-id
 *     surfaces). Lookups carry lifecycle uncertainty: a registry
 *     slot may recycle between ticks, and @ref freeze may have
 *     dropped a previously-known id since the caller last looked
 *     it up. The engine-token wrapper exposes these through
 *     @ref vigine::engine::Result so callers branch on a typed
 *     reason; the @ref IContext API itself returns the underlying
 *     handle (@c std::shared_ptr / reference) directly because
 *     services using @ref IContext during onInit run before any
 *     state transitions can invalidate them.
 *   - @b Infrastructure accessors -- @ref threadManager,
 *     @ref systemBus, @ref stateMachine, @ref taskFlow. These
 *     resources outlive every state transition (the context owns
 *     them for the engine's whole lifetime), so they are always
 *     non-gated and return references directly.
 *
 * Strict construction order (AD-5 C8, encoded by @ref AbstractContext):
 *   1. @ref core::threading::IThreadManager (created first).
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
    [[nodiscard]] virtual core::threading::IThreadManager &threadManager() = 0;

    // ------ Engine environment (default-built; replaceable via setter) ------

    /**
     * @brief Returns the engine-wide entity manager.
     *
     * The aggregator builds a default @ref IEntityManager during
     * construction so every task observes a live manager through
     * @c apiToken()->entityManager() without anyone wiring it up
     * explicitly. Applications that need a different concrete
     * implementation replace the slot through @ref setEntityManager;
     * the prior owner is destroyed via the unique_ptr slot's RAII
     * chain. The reference is valid for the context's lifetime
     * regardless of how many overrides have happened.
     */
    [[nodiscard]] virtual IEntityManager &entityManager() = 0;

    /**
     * @brief Replaces the default entity manager with @p entityManager.
     *
     * The prior owner (whatever the constructor or a previous
     * @ref setEntityManager call left in the slot) is destroyed in
     * place by the unique_ptr's RAII chain. Passing a null pointer is
     * implementation-defined; the default concrete asserts on null in
     * Debug and treats null as a no-op in Release so the "the
     * accessor never returns a null reference" contract holds.
     */
    virtual void
        setEntityManager(std::unique_ptr<IEntityManager> entityManager) = 0;

    /**
     * @brief Returns the engine-wide signal-emitter facade.
     *
     * The aggregator builds a default @ref messaging::ISignalEmitter
     * during construction so every task observes a live emitter
     * through @c apiToken()->signalEmitter() without anyone wiring it
     * up explicitly. Applications that need a different concrete
     * implementation replace the slot through @ref setSignalEmitter;
     * the prior owner is destroyed via the unique_ptr slot's RAII
     * chain. The reference is valid for the context's lifetime.
     */
    [[nodiscard]] virtual messaging::ISignalEmitter &signalEmitter() = 0;

    /**
     * @brief Replaces the default signal emitter with @p signalEmitter.
     *
     * Same RAII-replacement contract as @ref setEntityManager.
     */
    virtual void
        setSignalEmitter(std::unique_ptr<messaging::ISignalEmitter> signalEmitter) = 0;

    /**
     * @brief Returns the engine that owns this context.
     *
     * The reference is the back-pointer the engine wires in through
     * its constructor body so tasks reaching the engine surface
     * through @c apiToken()->engine() see the same object that
     * created the context. The reference is valid for the engine's
     * entire lifetime.
     */
    [[nodiscard]] virtual engine::IEngine &engine() = 0;

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
     * @brief Registers @p service on the context with a fresh
     *        auto-allocated id.
     *
     * The context stamps the service with a generational
     * @ref service::ServiceId and stores it in its registry. Callers
     * obtain the stamped id through @c service->id() after the call
     * returns success. Returns @ref Result::Code::TopologyFrozen when
     * the context has been frozen; returns @ref Result::Code::Error
     * when @p service is @c nullptr. The service is held by
     * @c std::shared_ptr inside the registry; callers may keep or
     * drop their own handle.
     */
    [[nodiscard]] virtual Result
        registerService(std::shared_ptr<service::IService> service) = 0;

    /**
     * @brief Registers @p service under the caller-provided
     *        @p knownId, replacing any existing slot occupant.
     *
     * Used for the well-known id pattern (see
     * @c include/vigine/api/service/wellknown.h): the caller declares
     * a stable @ref service::ServiceId constant, registers a service
     * implementation under that id, and every task resolves the
     * service through the same constant via @c apiToken()->service.
     * Both engine-scope (@c platformService, @c graphicsService) and
     * application-scope ids use this overload; the engine reserves
     * @c index in [1..15] for its own well-known slots, leaving
     * @c index >= 16 free for application-scope ids.
     *
     * @param service A non-null @c std::shared_ptr to the service.
     *        Must derive from @ref service::AbstractService so the
     *        registry can stamp the @p knownId on the service via
     *        @ref service::AbstractService::setId.
     * @param knownId The caller-provided id. Must satisfy
     *        @ref service::ServiceId::valid (non-zero generation).
     * @return @ref Result::Code::Success on registration success.
     *         @ref Result::Code::TopologyFrozen when the context has
     *         been frozen. @ref Result::Code::Error when @p service is
     *         null, when @p knownId is the invalid sentinel, or when
     *         @p service is not derived from
     *         @ref service::AbstractService (no setId path).
     *
     * Replacement semantics: if the slot keyed by @p knownId.index
     * already holds a service (engine-built default or a prior
     * caller-side registration), that occupant is destroyed via its
     * @c shared_ptr count dropping to zero (assuming the caller did
     * not keep an external handle). Callers that intend to reuse the
     * prior occupant should hold their own @c shared_ptr before
     * calling this overload.
     */
    [[nodiscard]] virtual Result
        registerService(std::shared_ptr<service::IService> service,
                        service::ServiceId                 knownId) = 0;

    // ------ Engine-token factory ------

    /**
     * @brief Mints a fresh @ref vigine::engine::IEngineToken bound to
     *        @p boundState.
     *
     * The returned token is the state-scoped DI surface a task receives
     * during @c onEnter. For domain accessors it calls this aggregator's
     * non-gated accessors (which return @c std::shared_ptr / a raw
     * reference) and wraps the outcome in a @ref vigine::engine::Result
     * -- adding the alive-state gate that flips to
     * @ref vigine::engine::Result::Code::Expired after invalidation. For
     * infrastructure accessors it forwards directly (ungated, raw
     * reference) per the R-StateScope hybrid policy described on the
     * class docstring.
     *
     * The token observes the engine's @ref vigine::statemachine::IStateMachine
     * for invalidation: when the FSM transitions out of @p boundState,
     * the token's gated accessors start reporting
     * @ref vigine::engine::Result::Code::Expired and any callback
     * registered through @c subscribeExpiration fires exactly once. The
     * ungated accessors keep working until the token is destroyed so
     * an invalidated task can still drain in-flight scheduling.
     *
     * Ownership: the caller owns the returned @c std::unique_ptr.
     * Tasks typically receive the token by reference through their
     * wiring path; the state machine keeps the returned unique_ptr
     * alive for the duration of the bound state.
     *
     * Lifetime invariants of the token wiring:
     *   - The token captures a non-owning reference to this context
     *     and to its @ref stateMachine. Both must outlive the token;
     *     the engine's strict construction order guarantees this
     *     (the context owns the state machine; both are torn down
     *     after every token has been dropped).
     *   - When the engine-wide signal-emitter façade has not yet been
     *     wired through @ref IContext (the wrapper follow-up under the
     *     #197 umbrella ships separately), the token falls back to a
     *     file-private @c NullSignalEmitter stub so the ungated
     *     @c signalEmitter accessor honours its "cannot fail" contract
     *     even in the unwired configuration. Once the façade lands,
     *     this factory passes the real wrapper through and the stub
     *     stays uninstantiated.
     *
     * @param boundState State the new token is bound to. The token
     *        does not validate that @p boundState is registered on
     *        the FSM; passing an unregistered id yields a token that
     *        observes the FSM but never matches any transition --
     *        effectively a permanently-alive handle. Callers that
     *        want a stricter contract pre-validate the id through
     *        @ref stateMachine.
     * @return A live token from the aggregator
     *         (@ref vigine::context::AbstractContext path). Token
     *         construction is allocation-only and any
     *         @c std::bad_alloc surfaces as an exception per the
     *         standard library contract. Callers reach a context
     *         carrying a live @ref vigine::statemachine::IStateMachine
     *         through @c vigine::context::createContext.
     */
    [[nodiscard]] virtual std::unique_ptr<vigine::engine::IEngineToken>
        makeEngineToken(vigine::statemachine::StateId boundState) = 0;

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

#pragma once

/**
 * @file enginetoken.h
 * @brief Concrete final implementation of @ref vigine::engine::IEngineToken.
 *
 * @ref vigine::engine::EngineToken is the @c final tier of the engine-
 * token recipe. It seals the inheritance chain over
 * @ref vigine::engine::AbstractEngineToken (which carries the
 * @c boundState id and the @c alive atomic flag) and supplies bodies
 * for every accessor the contract still leaves pure virtual:
 *   - Domain-level accessors (@ref service, @ref system,
 *     @ref entityManager, @ref components, @ref ecs) are gated.
 *     They consult @ref isAlive first and short-circuit to
 *     @ref Result::Code::Expired the moment the FSM has left the
 *     state the token was bound to. While the token is still live,
 *     they delegate to the engine's @ref vigine::IContext for the
 *     actual lookup and translate the lookup outcome into the
 *     @ref Result wrapper.
 *   - Infrastructure accessors (@ref threadManager, @ref systemBus,
 *     @ref signalEmitter, @ref stateMachine) are ungated. The
 *     resources they expose live for the engine's lifetime, so the
 *     token forwards them straight from the context regardless of
 *     the alive flag — the hybrid-gating policy from the
 *     R-StateScope rule.
 *   - @ref subscribeExpiration registers a one-shot callback fired
 *     when the FSM transitions away from the bound state. The
 *     callback runs on the controller thread synchronously inside
 *     the @c AbstractStateMachine listener firing path and BEFORE
 *     the alive flag is observed cleared by future readers. When the
 *     token is already invalidated at registration time the
 *     callback fires immediately (defensive same-thread invocation)
 *     and the returned subscription token is inert.
 *
 * Lifetime and ownership:
 *   - The state machine owns the token instance. Clients never
 *     construct one directly; they receive a reference through the
 *     task wiring path that a follow-up leaf finalises. The token
 *     keeps a non-owning reference to the engine's
 *     @ref vigine::IContext and a non-owning reference to the
 *     @ref vigine::statemachine::IStateMachine the token registered
 *     its invalidation listener on. Both references must outlive
 *     the token; the engine's strict construction order makes that
 *     the natural arrangement.
 *
 * Thread-safety:
 *   - @ref isAlive reads happen-before any side effect the
 *     invalidation listener publishes (acquire / release fence on
 *     the alive flag, inherited from @ref AbstractEngineToken).
 *   - @ref subscribeExpiration acquires the token's internal mutex
 *     for the duration of the callback-list mutation only. The
 *     listener firing path takes the same mutex briefly to swap the
 *     callback list aside, then runs the callbacks outside the
 *     lock so a callback that itself calls @ref subscribeExpiration
 *     does not deadlock.
 */

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "vigine/api/engine/abstractengine_token.h"
#include "vigine/api/engine/iengine_token.h"
#include "vigine/messaging/isubscriptiontoken.h"
#include "vigine/statemachine/stateid.h"

namespace vigine
{
class IContext;
class IEntityManager;
class IComponentManager;
} // namespace vigine

namespace vigine::ecs
{
class IECS;
class ISystem;
} // namespace vigine::ecs

namespace vigine::messaging
{
class IMessageBus;
} // namespace vigine::messaging

namespace vigine::service
{
class IService;
} // namespace vigine::service

namespace vigine::signalemitter
{
class ISignalEmitter;
} // namespace vigine::signalemitter

namespace vigine::statemachine
{
class IStateMachine;
} // namespace vigine::statemachine

namespace vigine::core::threading
{
class IThreadManager;
} // namespace vigine::core::threading

namespace vigine::engine
{

/**
 * @brief Concrete state-scoped @c final implementation of
 *        @ref IEngineToken.
 *
 * Constructed with a back-reference to the engine's
 * @ref vigine::IContext and the @ref vigine::statemachine::IStateMachine
 * the token observes for invalidation. The constructor registers the
 * token as a listener on the state machine's invalidation registry so
 * the FSM-level transition path (sync @c transition() and async
 * @c processQueuedTransitions() drain alike) drives the token's alive
 * flag and the registered expiration callbacks without the token
 * needing to poll.
 */
class EngineToken final : public AbstractEngineToken
{
  public:
    /**
     * @brief Builds a token bound to @p boundState and observing
     *        @p stateMachine.
     *
     * The token captures @p context and @p stateMachine by reference
     * and registers itself as an invalidation listener. The listener
     * subscription is owned by the token and torn down in the
     * destructor; dropping the token therefore detaches the listener
     * from the state machine cleanly even if the token outlives every
     * caller-supplied @ref subscribeExpiration token.
     *
     * @p context, @p stateMachine, and @p signalEmitter must outlive
     * the @ref EngineToken instance. The engine's strict construction
     * order makes this the natural arrangement: the context owns the
     * state machine and the signal emitter; the token is owned (or
     * referenced) by the state machine; both die before the context.
     *
     * The optional @p signalEmitter pointer carries the
     * @ref ISignalEmitter façade the @ref signalEmitter accessor
     * returns. When the engine wiring does not yet expose one
     * (the signal-emitter follow-up under #197 lands separately)
     * pass @c nullptr and the @ref signalEmitter accessor will
     * @c std::abort the program if a caller invokes it; the gated /
     * ungated lifecycle the rest of the surface exercises stays
     * functional regardless.
     */
    EngineToken(vigine::statemachine::StateId         boundState,
                vigine::IContext                     &context,
                vigine::statemachine::IStateMachine  &stateMachine,
                vigine::signalemitter::ISignalEmitter *signalEmitter = nullptr);

    ~EngineToken() override;

    EngineToken(const EngineToken &)            = delete;
    EngineToken &operator=(const EngineToken &) = delete;
    EngineToken(EngineToken &&)                 = delete;
    EngineToken &operator=(EngineToken &&)      = delete;

    // ------ IEngineToken: gated domain accessors ------

    [[nodiscard]] Result<vigine::service::IService &>
        service(vigine::service::ServiceId id) override;

    [[nodiscard]] Result<vigine::ecs::ISystem &>
        system(vigine::SystemId id) override;

    [[nodiscard]] Result<vigine::IEntityManager &> entityManager() override;

    [[nodiscard]] Result<vigine::IComponentManager &> components() override;

    [[nodiscard]] Result<vigine::ecs::IECS &> ecs() override;

    // ------ IEngineToken: ungated infrastructure accessors ------

    [[nodiscard]] vigine::core::threading::IThreadManager &
        threadManager() noexcept override;

    [[nodiscard]] vigine::messaging::IMessageBus &systemBus() noexcept override;

    [[nodiscard]] vigine::signalemitter::ISignalEmitter &
        signalEmitter() noexcept override;

    [[nodiscard]] vigine::statemachine::IStateMachine &
        stateMachine() noexcept override;

    // ------ IEngineToken: expiration notification ------

    [[nodiscard]] std::unique_ptr<vigine::messaging::ISubscriptionToken>
        subscribeExpiration(std::function<void()> callback) override;

  private:
    /**
     * @brief Slot in the expiration-callback registry.
     *
     * A monotonic @c id keys each registration so the matching
     * subscription token can find its slot back without iterating by
     * callback identity. An empty @c callback marks the slot cancelled
     * and is skipped when the token fires.
     */
    struct ExpirationSlot
    {
        std::uint32_t         id{0};
        std::function<void()> callback;
    };

    /**
     * @brief Listener body the constructor registers on the FSM.
     *
     * Called by @ref AbstractStateMachine::fireInvalidationListeners
     * with the id of the state the FSM is about to leave. When that
     * id matches @ref boundState the body flips the token to expired
     * (through @ref AbstractEngineToken::markExpired) and fires every
     * registered expiration callback exactly once.
     */
    void onStateInvalidated(vigine::statemachine::StateId leavingState);

    /**
     * @brief Fires every active expiration callback exactly once.
     *
     * Idempotent: a second call observes the @ref _expirationFired
     * latch already true and returns. Callbacks run outside
     * @ref _expirationMutex so a callback that re-enters the token
     * (e.g. registers another expiration listener) does not deadlock.
     */
    void fireExpirationCallbacks();

    /**
     * @brief Removes the slot whose id matches @p id from
     *        @ref _expirationCallbacks.
     *
     * Idempotent on stale ids. Called by the @ref ExpirationToken
     * destructor and by its @c cancel.
     */
    void cancelExpirationCallback(std::uint32_t id) noexcept;

    /**
     * @brief Subscription token returned by @ref subscribeExpiration.
     */
    class ExpirationToken final : public vigine::messaging::ISubscriptionToken
    {
      public:
        ExpirationToken(EngineToken *owner, std::uint32_t id) noexcept;
        ~ExpirationToken() override;

        void               cancel() noexcept override;
        [[nodiscard]] bool active() const noexcept override;

        ExpirationToken(const ExpirationToken &)            = delete;
        ExpirationToken &operator=(const ExpirationToken &) = delete;
        ExpirationToken(ExpirationToken &&)                 = delete;
        ExpirationToken &operator=(ExpirationToken &&)      = delete;

      private:
        EngineToken               *_owner;
        std::atomic<std::uint32_t> _id;
    };

    vigine::IContext                       &_context;
    vigine::statemachine::IStateMachine    &_stateMachine;
    vigine::signalemitter::ISignalEmitter *_signalEmitter;

    /**
     * @brief Subscription that drives the token's invalidation
     *        listener on the state machine.
     *
     * The token owns this handle so the listener detaches cleanly
     * either when the destructor runs or when @c cancel is called
     * explicitly. The listener body runs on the controller thread
     * driven by the FSM transition path.
     */
    std::unique_ptr<vigine::messaging::ISubscriptionToken> _invalidationSub;

    mutable std::mutex          _expirationMutex;
    std::vector<ExpirationSlot> _expirationCallbacks;
    std::atomic<std::uint32_t>  _nextExpirationId{1};

    /**
     * @brief Latch that marks the expiration callbacks as already
     *        fired.
     *
     * Set under @ref _expirationMutex when @ref fireExpirationCallbacks
     * runs the first time. Subsequent registrations that observe a
     * already-true latch fire the new callback inline (defensive
     * same-thread invocation), matching the contract documented on
     * @ref subscribeExpiration.
     */
    std::atomic<bool> _expirationFired{false};
};

} // namespace vigine::engine

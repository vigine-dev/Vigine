#pragma once

/**
 * @file iengine_token.h
 * @brief State-scoped handle to the engine API used by @c ITask clients.
 *
 * The state machine issues an @ref vigine::engine::IEngineToken to every
 * task that runs inside a given state. The token behaves like
 * @c std::weak_ptr: it resolves to a live view of the engine API while
 * the state it was bound to is active, and it reports expired as soon as
 * the FSM leaves that state. The concrete implementation, the factory on
 * @ref vigine::IContext, and the FSM-level invalidation hook land in
 * follow-up issues under the #197 umbrella. This header ships the pure
 * contract only.
 *
 * Separation between gated and ungated accessors:
 *   - Domain-level accessors (@ref service, @ref system,
 *     @ref entityManager, @ref components, @ref ecs) carry lifecycle
 *     uncertainty: the underlying registry slot may recycle between
 *     ticks, and a stale handle must not fall back to a different
 *     object. They return an @ref vigine::engine::Result wrapper that
 *     carries either a live reference or the reason the lookup failed.
 *   - Infrastructure accessors (@ref threadManager, @ref systemBus,
 *     @ref signalEmitter, @ref stateMachine) refer to engine-lifetime
 *     singletons that outlive every state transition. They return the
 *     underlying reference directly and cannot fail.
 *
 * Invariants:
 *   - INV-10: @c I prefix for this pure-virtual interface (no state,
 *             no non-virtual method bodies).
 *   - INV-12: no data members on this pure interface; the stateful
 *             base @ref vigine::engine::AbstractEngineToken carries
 *             @c boundState and @c alive.
 *
 * INV-1 deviation -- explicit, scoped exemption:
 *   The @ref vigine::engine::Result wrapper is a tiny value template
 *   used exclusively as the return type of the gated accessors. It
 *   exists so a single return site can convey "live reference or
 *   reason-for-failure" without collapsing onto a nullable pointer
 *   and without minting a bespoke enum per accessor. Callers never
 *   name the template on anything other than the gated-accessor
 *   return types; no method on the interface itself takes a template
 *   parameter. This deviation is sanctioned by the #220 scope note;
 *   the rest of the token surface remains non-template, non-templated,
 *   and INV-1-conforming.
 */

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>

#include "vigine/messaging/isubscriptiontoken.h"
#include "vigine/api/service/serviceid.h"
#include "vigine/statemachine/stateid.h"

// Forward declarations for the references the gated and ungated
// accessors return. Kept as forward declarations so this header does
// not pull the full wrapper or facade trees into every task's
// translation unit. The implementation file (shipping in a follow-up
// issue) includes the real surfaces.

namespace vigine
{
// Legacy string-based SystemId lives in the top-level vigine namespace
// today (see include/vigine/ecs/abstractsystem.h). The token surface
// accepts it here so the interface compiles against the current tree.
// When the ECS wrapper follow-up migrates SystemId into
// vigine::ecs::SystemId, a later leaf updates this signature together
// with every concrete EngineToken.
using SystemId = std::string;
} // namespace vigine

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

namespace vigine::signalemitter
{
class ISignalEmitter;
} // namespace vigine::signalemitter

namespace vigine::statemachine
{
class IStateMachine;
} // namespace vigine::statemachine

namespace vigine::threading
{
class IThreadManager;
} // namespace vigine::threading

namespace vigine
{
// Forward declarations for types that the gated accessors reference
// but that do not yet exist in the public tree. Each stub is marked
// with the follow-up issue that finalises the surface.
class IEntityManager;          // defined in vigine/ecs/ientitymanager.h today.
class IComponentManager;       // to be defined under #197 (component-manager wrapper leaf).
} // namespace vigine

namespace vigine::ecs
{
class ISystem;                 // to be defined under #197 (system wrapper leaf).
} // namespace vigine::ecs

namespace vigine::engine
{

/**
 * @brief Thin value wrapper carrying either a live reference of type
 *        @p T or the reason the lookup failed.
 *
 * @ref Result is the return type of every gated accessor on
 * @ref IEngineToken. It exists so callers can branch on one typed
 * answer instead of juggling a nullable pointer next to an error code.
 * The wrapper never owns the referred-to object; ownership stays with
 * the engine's context aggregator.
 *
 * Status values:
 *   - @ref Code::Ok          -- the reference is live.
 *   - @ref Code::Expired     -- the token is no longer bound to an
 *                               active state; callers must drop the
 *                               token.
 *   - @ref Code::NotFound    -- the id / slot addressed a recycled or
 *                               unregistered entry.
 *   - @ref Code::Unavailable -- the underlying subsystem is still
 *                               initialising or has already been torn
 *                               down; callers can retry.
 *
 * The template parameter @p T is always instantiated with a reference
 * type on the IEngineToken API (e.g. @c Result<IService&>). Storing a
 * raw pointer internally is safe because the referent is owned by the
 * engine; the wrapper is a read-only snapshot of the lookup outcome.
 *
 * Thread-safety: the wrapper is trivially copyable and carries no
 * synchronisation primitives. Concurrent reads from different threads
 * are well-defined; mutating the wrapper is not supported after
 * construction.
 *
 * @note Introduced with explicit scope as a return-type utility for
 *       @ref IEngineToken; see the INV-1 deviation note on the file
 *       docstring. Not intended to replace @ref vigine::Result on the
 *       rest of the engine surface.
 */
template <typename T>
class Result
{
  public:
    enum class Code : std::uint8_t
    {
        Ok,
        Expired,
        NotFound,
        Unavailable,
    };

    /**
     * @brief Constructs an @c Ok result pointing at @p target.
     *
     * The reference must remain live for the duration of the
     * caller's access window. The engine guarantees this by
     * pinning the referent until the next state transition.
     */
    [[nodiscard]] static Result ok(T target) noexcept
    {
        using Stored = std::remove_reference_t<T>;
        return Result{Code::Ok, std::addressof(static_cast<Stored &>(target))};
    }

    /**
     * @brief Constructs a failure result carrying the @p code reason.
     */
    [[nodiscard]] static Result failure(Code code) noexcept
    {
        return Result{code, nullptr};
    }

    /**
     * @brief Returns the status code.
     */
    [[nodiscard]] Code code() const noexcept { return _code; }

    /**
     * @brief Returns @c true when @ref code is @c Code::Ok.
     */
    [[nodiscard]] bool ok() const noexcept { return _code == Code::Ok; }

    /**
     * @brief Returns the referred-to object.
     *
     * Caller-side pre-condition: @ref ok returns @c true. Calling
     * this accessor on a failure result is a programming error and
     * dereferences a null pointer -- the contract is that callers
     * always branch on @ref ok first.
     */
    [[nodiscard]] T value() const noexcept { return *_target; }

  private:
    using Stored = std::remove_reference_t<T>;

    Result(Code code, Stored *target) noexcept : _code(code), _target(target) {}

    Code     _code;
    Stored  *_target;
};

/**
 * @brief Pure-virtual state-scoped handle to the engine API.
 *
 * A token is issued by the state machine when a task enters a state
 * and is handed to that task through @c ITask::onEnter. While the
 * state stays active, the token's accessors resolve normally. The
 * moment the FSM transitions away from the bound state, every live
 * token binds to the new state is invalidated: gated accessors start
 * reporting @ref Result::Code::Expired and @ref isAlive flips to
 * @c false.
 *
 * Lifetime:
 *   - The concrete @c EngineToken object is owned by the state
 *     machine. The state machine hands tasks a reference, not a
 *     value; tasks never construct or destroy a token.
 *   - @ref subscribeExpiration returns an RAII
 *     @ref vigine::messaging::ISubscriptionToken. Dropping the
 *     returned token detaches the callback without racing the
 *     state transition.
 *
 * Thread-safety: every accessor is safe to call from any thread. The
 * gated accessors read @ref isAlive via the token's internal atomic
 * before delegating to the engine's context aggregator; the delegation
 * itself is guarded by the context's own locking rules.
 *
 * Invariants:
 *   - INV-10: @c I prefix for this pure-virtual interface (no state,
 *             no non-virtual method bodies).
 *   - INV-12: no data members; state lives on
 *             @ref AbstractEngineToken.
 */
class IEngineToken
{
  public:
    virtual ~IEngineToken() = default;

    // ------ Introspection ------

    /**
     * @brief Returns the state this token was bound to at issue time.
     *
     * The value is immutable for the token's lifetime. A task can
     * compare it against the current active state (via
     * @ref stateMachine) to decide whether to skip a side effect
     * that depends on the originating context.
     */
    [[nodiscard]] virtual vigine::statemachine::StateId
        boundState() const noexcept = 0;

    /**
     * @brief Reports whether the bound state is still active.
     *
     * Reads are lock-free. A @c false return guarantees that every
     * subsequent gated accessor reports
     * @ref Result::Code::Expired without touching the engine.
     */
    [[nodiscard]] virtual bool isAlive() const noexcept = 0;

    // ------ Domain-level accessors (gated) ------

    /**
     * @brief Resolves the service registered under @p id.
     *
     * Reports @ref Result::Code::Expired when the token's bound
     * state is no longer active, @ref Result::Code::NotFound when
     * @p id is the invalid sentinel or addresses a recycled slot,
     * and @ref Result::Code::Ok otherwise.
     */
    [[nodiscard]] virtual Result<vigine::service::IService &>
        service(vigine::service::ServiceId id) = 0;

    /**
     * @brief Resolves the system registered under @p id.
     *
     * The concrete @ref vigine::ecs::ISystem interface lands in a
     * follow-up issue; the signature already commits to the
     * eventual surface so call sites do not rewrite when the
     * follow-up merges.
     */
    [[nodiscard]] virtual Result<vigine::ecs::ISystem &>
        system(vigine::SystemId id) = 0;

    /**
     * @brief Resolves the engine-wide entity manager.
     */
    [[nodiscard]] virtual Result<vigine::IEntityManager &> entityManager() = 0;

    /**
     * @brief Resolves the engine-wide component manager.
     *
     * The concrete @ref vigine::IComponentManager interface lands
     * in a follow-up issue; the signature already commits to the
     * eventual surface.
     */
    [[nodiscard]] virtual Result<vigine::IComponentManager &> components() = 0;

    /**
     * @brief Resolves the engine-wide ECS wrapper.
     */
    [[nodiscard]] virtual Result<vigine::ecs::IECS &> ecs() = 0;

    // ------ Infrastructure accessors (ungated) ------

    /**
     * @brief Returns the engine-wide thread manager.
     *
     * The thread manager is built in the first step of the context
     * aggregator's construction chain and outlives every token the
     * state machine hands out. No gate is needed.
     */
    [[nodiscard]] virtual vigine::threading::IThreadManager &
        threadManager() noexcept = 0;

    /**
     * @brief Returns the engine-wide system bus.
     */
    [[nodiscard]] virtual vigine::messaging::IMessageBus &
        systemBus() noexcept = 0;

    /**
     * @brief Returns the engine-wide signal emitter facade.
     */
    [[nodiscard]] virtual vigine::signalemitter::ISignalEmitter &
        signalEmitter() noexcept = 0;

    /**
     * @brief Returns the engine-wide state machine wrapper.
     */
    [[nodiscard]] virtual vigine::statemachine::IStateMachine &
        stateMachine() noexcept = 0;

    // ------ Expiration notification ------

    /**
     * @brief Subscribes @p callback to the token's expiration event.
     *
     * The engine invokes @p callback exactly once, on a worker
     * thread picked by the engine's thread manager, when the bound
     * state transitions away. Dropping the returned subscription
     * token before expiration detaches the callback cleanly; after
     * expiration the returned token is inert.
     *
     * Returns a null subscription token when @p callback is empty
     * or when the token is already expired at registration time.
     */
    [[nodiscard]] virtual std::unique_ptr<vigine::messaging::ISubscriptionToken>
        subscribeExpiration(std::function<void()> callback) = 0;

    IEngineToken(const IEngineToken &)            = delete;
    IEngineToken &operator=(const IEngineToken &) = delete;
    IEngineToken(IEngineToken &&)                 = delete;
    IEngineToken &operator=(IEngineToken &&)      = delete;

  protected:
    IEngineToken() = default;
};

} // namespace vigine::engine

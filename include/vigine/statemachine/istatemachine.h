#pragma once

#include <thread>

#include "vigine/result.h"
#include "vigine/statemachine/routemode.h"
#include "vigine/statemachine/stateid.h"

namespace vigine
{
/**
 * @brief Pure-virtual forward-declared stub for the legacy state-machine
 *        surface.
 *
 * @ref IStateMachine is a minimal stub whose only contract is a virtual
 * destructor. It exists so that @ref Engine::state can return a
 * reference to a pure-virtual interface without requiring the legacy
 * @c StateMachine (declared in @c include/vigine/statemachine.h) to
 * migrate onto the new wrapper surface in this leaf. The richer
 * wrapper surface — state registration, hierarchical routing, and so
 * on — lives under @c vigine::statemachine::IStateMachine in the
 * nested namespace block below; a later leaf moves the legacy
 * @c StateMachine onto that surface and retires this stub.
 *
 * Ownership: this type is never instantiated directly. The concrete
 * legacy @c StateMachine object derives from it and is owned by the
 * @ref Engine as @c std::unique_ptr.
 */
class IStateMachine
{
  public:
    virtual ~IStateMachine() = default;

    IStateMachine(const IStateMachine &)            = delete;
    IStateMachine &operator=(const IStateMachine &) = delete;
    IStateMachine(IStateMachine &&)                 = delete;
    IStateMachine &operator=(IStateMachine &&)      = delete;

  protected:
    IStateMachine() = default;
};

} // namespace vigine

namespace vigine::statemachine
{
/**
 * @brief Pure-virtual Level-1 wrapper surface for the state machine.
 *
 * @ref IStateMachine is the user-facing contract over the state
 * machine substrate: it registers states, wires hierarchical
 * parent/child relationships, picks the initial state, changes the
 * currently active state, and walks the hierarchy. The interface
 * knows nothing about the underlying graph storage; substrate
 * primitive types never appear in the public API per INV-11. The
 * stateful base @ref AbstractStateMachine carries an opaque internal
 * state topology through a private @c std::unique_ptr so the
 * substrate stays hidden from consumers of this header.
 *
 * Ownership and lifetime:
 *   - Concrete state machines are constructed through the
 *     non-template factory in @ref factory.h and handed back as
 *     @c std::unique_ptr<IStateMachine>. The caller owns the returned
 *     pointer.
 *   - States are value handles (@ref StateId); the machine never
 *     hands out raw pointers to its internal state slots. Registering
 *     a state yields its generational @ref StateId; the handle stays
 *     stable until the machine is destroyed.
 *   - The machine auto-provisions one default state in its
 *     constructor per UD-3: callers that register no states still
 *     observe a valid @ref current state and a valid initial state.
 *     Callers that register their own states freely override the
 *     initial selection with @ref setInitial.
 *
 * Routing model (UD-3):
 *   - @ref routeMode reports the hierarchical routing strategy the
 *     machine uses when a later leaf wires it to the message bus.
 *     @ref RouteMode::Bubble is the default: events that the active
 *     child state does not handle bubble up the @c ChildOf parent
 *     chain until a handler consumes them or the root is reached.
 *     Flat FSM users select @ref RouteMode::Direct to keep the legacy
 *     behaviour.
 *
 * Thread-safety: the contract does not fix one. The default
 * implementation inherits the substrate's reader-writer policy; the
 * concrete machine exposed through @c createStateMachine serialises
 * mutations with the same @c std::shared_mutex the underlying graph
 * uses. Concurrent queries are safe with each other; concurrent
 * mutations take the exclusive lock.
 *
 * INV-1 compliance: the surface uses no template parameters. INV-10
 * compliance: the name carries the @c I prefix for a pure-virtual
 * interface. INV-11 compliance: the public API mentions only state
 * machine domain handles (@ref StateId, @ref RouteMode, @ref Result);
 * no graph primitive types cross the boundary.
 */
class IStateMachine
{
  public:
    virtual ~IStateMachine() = default;

    // ------ State registration ------

    /**
     * @brief Allocates a fresh state slot and returns its generational
     *        handle.
     *
     * The returned handle is always valid. Every subsequent query
     * (@ref hasState, @ref parent, @ref isAncestorOf, @ref current)
     * accepts the returned id; recycled slots carry a bumped
     * generation so stale handles fail safely.
     */
    [[nodiscard]] virtual StateId addState() = 0;

    /**
     * @brief Reports whether the state machine currently tracks the
     *        state addressed by @p state.
     *
     * Useful for pre-flight checks in callers that hold a cached
     * @ref StateId — the wrapper surface is append-only within a
     * session (there is no `removeState()` surface today), so a
     * false result means either the id was never issued by this
     * machine or its generation has been invalidated by a future
     * recycle path. When @ref StateId state removal does land, this
     * doc should be revised to cover the stale-id case explicitly.
     */
    [[nodiscard]] virtual bool hasState(StateId state) const noexcept = 0;

    // ------ Hierarchy ------

    /**
     * @brief Wires @p child under @p parent in the hierarchical
     *        topology.
     *
     * Both states must have been registered beforehand. Adding the
     * edge records a directed @c ChildOf relationship that
     * @ref parent and @ref isAncestorOf query later. Reports
     * @ref Result::Code::Error when either state is stale or when the
     * edge would introduce a cycle.
     *
     * Multiple children under the same parent are allowed; that is
     * how the machine models parallel sub-states when a later leaf
     * adds FanOut routing.
     */
    virtual Result addChildState(StateId parent, StateId child) = 0;

    /**
     * @brief Returns the parent of @p state or a default-constructed
     *        invalid @ref StateId when @p state is a root.
     *
     * Roots of the hierarchy (states without a @c ChildOf edge
     * pointing at them) report an invalid parent. The call is
     * @c const and takes the reader lock only; concurrent callers do
     * not serialise on one another.
     */
    [[nodiscard]] virtual StateId parent(StateId state) const = 0;

    /**
     * @brief Returns @c true when @p ancestor lies somewhere on the
     *        parent chain of @p descendant.
     *
     * The relation is strict: a state is @b not its own ancestor. An
     * invalid @p ancestor or @p descendant always returns @c false.
     * The walk is O(depth); callers that need repeated ancestor
     * checks for the same state cache the result.
     */
    [[nodiscard]] virtual bool isAncestorOf(StateId ancestor, StateId descendant) const = 0;

    // ------ Initial / current state ------

    /**
     * @brief Selects the initial (and currently active) state.
     *
     * The referenced state must have been registered through
     * @ref addState. Reports @ref Result::Code::Error when @p state
     * is stale; on success the next @ref current call returns
     * @p state.
     *
     * Per UD-3 every concrete machine auto-provisions one default
     * state in its constructor and selects it as the initial; callers
     * are free to override the selection with this call before they
     * start dispatching events.
     */
    virtual Result setInitial(StateId state) = 0;

    /**
     * @brief Transitions the machine to @p state as the new current
     *        state.
     *
     * The referenced state must have been registered. Reports
     * @ref Result::Code::Error when @p state is stale; on success the
     * next @ref current call returns @p state. The call does not
     * invoke enter/exit hooks by itself — those live on the state
     * base class that a later leaf adds; the wrapper only tracks
     * which state id is the active one.
     */
    virtual Result transition(StateId state) = 0;

    /**
     * @brief Returns the currently active state.
     *
     * Every concrete machine reports a valid id after construction
     * because the constructor auto-provisions the default state per
     * UD-3. A caller that never registers its own states still sees
     * the default state as the current one.
     */
    [[nodiscard]] virtual StateId current() const noexcept = 0;

    // ------ Routing ------

    /**
     * @brief Returns the hierarchical routing mode the machine uses.
     *
     * Defaults to @ref RouteMode::Bubble per UD-3. A later leaf that
     * wires the machine to the message bus consumes this value when
     * it resolves where an event goes; the wrapper in this leaf only
     * stores the closed enum so callers can inspect the chosen
     * strategy ahead of time.
     */
    [[nodiscard]] virtual RouteMode routeMode() const noexcept = 0;

    /**
     * @brief Replaces the hierarchical routing mode.
     *
     * Accepts any value of the closed @ref RouteMode enum; subsequent
     * @ref routeMode calls return the newly selected value.
     */
    virtual void setRouteMode(RouteMode mode) noexcept = 0;

    // ------ Thread affinity ------

    /**
     * @brief Binds the state machine to its controller thread.
     *
     * One-shot. Must be called once before the first sync mutation
     * (setInitial / transition / addChildState). A second call is
     * rejected: Debug builds assert; Release silently keeps the original
     * binding.
     */
    virtual void bindToControllerThread(std::thread::id controllerId) = 0;

    /**
     * @brief Returns the bound controller thread id.
     *
     * Default-constructed @c std::thread::id when not yet bound.
     */
    [[nodiscard]] virtual std::thread::id controllerThread() const noexcept = 0;

    // ------ Asynchronous transition request ------

    /**
     * @brief Request a transition from any thread.
     *
     * Thread-safe. Pushes @p target onto an internal FIFO queue under
     * an internal mutex; the call neither validates the id nor mutates
     * the active state. The request is applied later, on the controller
     * thread, by @ref processQueuedTransitions. Stale ids are
     * intentionally not surfaced: the drain delegates each entry to the
     * synchronous @ref transition call and discards its
     * @ref Result, so a stale @p target is silently dropped instead of
     * being reported back to the producer. Callers who need pre-flight
     * validation use @ref hasState before requesting; a separate future
     * leaf may add a diagnostic-sink callback API for visibility into
     * drain rejections.
     *
     * The call is idempotent in the sense that pushing the same target
     * twice queues two separate transitions. Multiple producers may
     * post concurrently; the queue mutex serialises pushes so the
     * observed FIFO order matches the order of mutex acquisitions.
     */
    virtual void requestTransition(StateId target) = 0;

    /**
     * @brief Drain the request queue on the controller thread.
     *
     * Must be called from the controller thread once a binding has
     * been installed via @ref bindToControllerThread. The thread
     * binding is one-shot and optional — when no binding has been
     * installed the gate is intentionally inactive (the
     * "un-bound = assert inactive" contract that the rest of the
     * sync mutators in this interface follow), so callers that opt
     * out of binding can call this method from any thread; once a
     * binding is in place a stray caller from another thread fires
     * the @c AbstractStateMachine::checkThreadAffinity contract
     * assert in Debug and is undefined behaviour in Release. Per-
     * request failures are intentionally not surfaced: the drain
     * discards the @ref Result returned by each delegated
     * @ref transition call. The drain takes one snapshot of the queue
     * under the queue mutex (atomically swapping it with an empty
     * deque) and then walks the snapshot in FIFO order, applying each
     * entry through @ref transition. The single-pass guarantee is
     * intentional: requests pushed by @c onEnter / @c onExit hooks
     * during the drain land on the live queue and are applied on the
     * @b next @ref processQueuedTransitions call, never inside the
     * same drain. That keeps the controller thread free of unbounded
     * reentry and gives state code a stable "tick" boundary.
     */
    virtual void processQueuedTransitions() = 0;

    IStateMachine(const IStateMachine &)            = delete;
    IStateMachine &operator=(const IStateMachine &) = delete;
    IStateMachine(IStateMachine &&)                 = delete;
    IStateMachine &operator=(IStateMachine &&)      = delete;

  protected:
    IStateMachine() = default;
};

} // namespace vigine::statemachine

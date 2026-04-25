#pragma once

#include <memory>
#include <thread>

#include "vigine/result.h"
#include "vigine/api/statemachine/routemode.h"
#include "vigine/api/statemachine/stateid.h"

namespace vigine
{
class TaskFlow;

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
 * Threading model — single controller thread + async drain (locked policy):
 *   - The machine has @b one controller thread, bound via
 *     @ref bindToControllerThread (one-shot, before the first sync
 *     mutation). Until the binding lands the controller-thread gate
 *     is intentionally inactive.
 *   - Synchronous mutators — @ref setInitial, @ref transition,
 *     @ref addState, @ref addChildState, @ref setRouteMode — and the
 *     drain pump @ref processQueuedTransitions are
 *     @b controller-thread-only once a binding is in place. Calls
 *     from any other thread fire the
 *     @c AbstractStateMachine::checkThreadAffinity contract assert in
 *     Debug builds and are undefined behaviour in Release.
 *   - Asynchronous transition requests via @ref requestTransition are
 *     thread-safe by construction and may be posted from
 *     @b any thread (controller, worker pool, message-bus delivery,
 *     OS callback). The post is fire-and-forget; the controller
 *     thread later applies the queued request through
 *     @ref processQueuedTransitions, which delegates to the
 *     synchronous @ref transition path on its own thread.
 *   - The owner of the controller-thread loop is responsible for
 *     calling @ref processQueuedTransitions on a periodic cadence
 *     while the machine is live, and once at teardown to drain
 *     whatever requests are still queued. The interface does not
 *     prescribe @e who that owner is; concrete embedders, hosting
 *     subsystems, or test harnesses each pick their own pump point.
 *   - Read-only queries (@ref hasState, @ref parent,
 *     @ref isAncestorOf, @ref current, @ref routeMode,
 *     @ref controllerThread) are safe to call from any thread and
 *     take only the substrate's reader lock or an atomic load.
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
     * @brief Synchronously transitions the machine to @p state as the
     *        new current state.
     *
     * @par Threading contract — controller-thread-only
     * This entry point is the @b synchronous half of the dual-API
     * transition surface. Per the locked policy it MUST be called only
     * from the controller thread that has been bound to the machine via
     * @ref bindToControllerThread. A call from any other thread fires
     * the @c AbstractStateMachine::checkThreadAffinity contract assert
     * in Debug builds and is undefined behaviour in Release. Code that
     * sits on a non-controller thread (worker pools, message-bus
     * delivery threads, OS callback threads) MUST NOT call
     * @ref transition directly — it queues a request through
     * @ref requestTransition instead and lets the controller-thread
     * drain pump apply it.
     *
     * @par Semantics
     * The referenced state must have been registered through
     * @ref addState. Reports @ref Result::Code::Error when @p state is
     * stale; on success the next @ref current call returns @p state.
     * The call applies the new state immediately and synchronously: by
     * the time @ref transition returns the active state has flipped and
     * every observer (the listener registry on
     * @ref AbstractStateMachine, future enter/exit hooks added by a
     * later leaf) has been invoked on the calling — controller —
     * thread. The wrapper itself only tracks which state id is active;
     * enter/exit hooks live on the state base class added by a future
     * leaf.
     *
     * @par Relationship to the async surface
     * @ref requestTransition is the asynchronous companion. Producers
     * on any thread post requests to a FIFO queue that the controller
     * thread later drains via @ref processQueuedTransitions; the drain
     * delegates each entry to @ref transition under the same
     * controller-thread-only contract. There is no other path to
     * mutate the active state — every mutation funnels through this
     * synchronous call site.
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
     * @par Two supported modes
     * The machine supports two mutually exclusive operation modes,
     * and the consumer chooses between them by either calling this
     * method or skipping it:
     *   - @b Bound mode (affinity gate active): the consumer calls
     *     @ref bindToControllerThread exactly once, before the first
     *     sync mutation (@ref setInitial, @ref transition,
     *     @ref addChildState) and before the first drain
     *     (@ref processQueuedTransitions). After the binding lands,
     *     every subsequent sync-mutator call is gated against
     *     @p controllerId via
     *     @c AbstractStateMachine::checkThreadAffinity. This is the
     *     mode production embedders pick when they want the assert
     *     to catch stray cross-thread mutations.
     *   - @b Unbound mode (affinity gate inactive): the consumer
     *     never calls @ref bindToControllerThread. The gate stays
     *     inactive for the life of the machine; sync mutators skip
     *     the assert and run on whichever thread issued them. This
     *     is the documented escape hatch for tests, ad-hoc embedders,
     *     and any caller that opts out of the dual-API policy and
     *     prefers to serialise mutations on its own.
     *
     * The two modes are not meant to be mixed: once the consumer
     * has decided to enable the gate via this call, the gate stays
     * active for the rest of the machine's life.
     *
     * @par Re-binding
     * A second call is rejected. The implementation installs the
     * binding via a compare-exchange against the default-constructed
     * @c std::thread::id sentinel, so a second attempt observes a
     * non-sentinel expected value and fails. Debug builds fire the
     * contract assert; Release silently keeps the original binding so
     * the machine remains usable but with the original controller.
     *
     * @par Thread-safety
     * Safe to call from any thread; the compare-exchange against the
     * controller-id atomic is the synchronisation point. The natural
     * call site is the controller thread itself, immediately after the
     * machine is constructed and before any worker thread can post a
     * request.
     */
    virtual void bindToControllerThread(std::thread::id controllerId) = 0;

    /**
     * @brief Returns the bound controller thread id.
     *
     * Default-constructed @c std::thread::id when no binding has been
     * installed yet. Reads use acquire semantics so the value reflects
     * the most recently published binding from
     * @ref bindToControllerThread. Safe to call from any thread.
     */
    [[nodiscard]] virtual std::thread::id controllerThread() const noexcept = 0;

    // ------ Asynchronous transition request ------

    /**
     * @brief Asynchronously requests a transition to @p target from
     *        any thread.
     *
     * @par Threading contract — any-thread, fire-and-forget
     * This entry point is the @b asynchronous half of the dual-API
     * transition surface. It is the ONLY way to drive a transition
     * from a non-controller thread. The call is thread-safe by
     * construction: it enqueues @p target onto an internal FIFO under
     * the queue's synchronisation primitive and returns immediately,
     * without validating the id, taking the topology lock, or touching
     * the active-state field. The controller thread later drains the
     * queue via @ref processQueuedTransitions and applies each entry
     * through the synchronous @ref transition call site, on its own
     * thread.
     *
     * @par No back-pressure to the producer
     * Stale ids and other failures are intentionally not surfaced to
     * the caller. The drain delegates each entry to @ref transition
     * and discards its @ref Result, so a stale @p target is silently
     * dropped instead of being reported back to the producer. Callers
     * that need pre-flight validation use @ref hasState before
     * requesting; a future diagnostic-sink callback API may add
     * visibility into drain rejections without changing the contract
     * here.
     *
     * @par Ordering and idempotency
     * The call is non-idempotent in effect: posting the same target
     * twice queues two separate transitions, both of which the drain
     * applies in order. Multiple producers may post concurrently;
     * pushes are serialised under the queue's synchronisation primitive,
     * so the observed FIFO order matches the order in which those
     * pushes were admitted. A request posted from inside an @c onEnter
     * / @c onExit hook (i.e. during a drain) sits on the @b live queue
     * and is applied on the @b next drain pass — the drain takes one
     * snapshot per call by design (see @ref processQueuedTransitions).
     *
     * @par Relationship to the sync surface
     * Code that already runs on the controller thread does not need to
     * route through this queue and can call @ref transition directly
     * for immediate effect. The async surface exists so worker pools,
     * message-bus delivery threads, OS callback threads, and any other
     * non-controller producer can drive transitions safely without
     * violating the controller-thread-only contract on
     * @ref transition.
     */
    virtual void requestTransition(StateId target) = 0;

    /**
     * @brief Drains the request queue on the controller thread.
     *
     * @par Threading contract — controller-thread drain pump
     * This is the controller-thread end of the async transition path.
     * Once a binding is in place via @ref bindToControllerThread the
     * call MUST run on the controller thread; a stray call from any
     * other thread fires the
     * @c AbstractStateMachine::checkThreadAffinity contract assert in
     * Debug and is undefined behaviour in Release. The "un-bound =
     * gate inactive" contract documented on
     * @ref bindToControllerThread applies here too: a consumer that
     * has opted into the unbound mode can call this method from any
     * thread.
     *
     * @par Pump shape — controller-thread owner drives the cadence
     * Calling this method is the responsibility of whichever component
     * owns the controller-thread loop. That owner is expected to drain
     * on a periodic cadence while the machine is live and once at
     * teardown (best-effort, draining whatever is still queued at stop
     * time). The interface does not name a specific owner; in
     * practice, embedders that host the FSM inside a tick loop pump
     * it from there, while tests and ad-hoc embedders pump it
     * explicitly when they need a deterministic boundary.
     *
     * @par Single-pass snapshot
     * The drain takes ONE snapshot of the queued requests per call by
     * atomically detaching the live queue (replacing it with an empty
     * one) under the queue's synchronisation primitive, then walks the
     * snapshot in FIFO order outside the lock. Each entry is applied
     * through the synchronous @ref transition call site, which means
     * listeners and (future) enter/exit hooks fire on the controller
     * thread inside this method. Per-entry failures are intentionally
     * not surfaced: the drain discards the @ref Result returned by
     * each delegated @ref transition.
     *
     * @par Re-entrancy boundary
     * The single-pass guarantee is deliberate. A request pushed from
     * inside an @c onEnter / @c onExit hook (or any listener) during
     * the drain lands on the live queue and is applied on the @b next
     * @ref processQueuedTransitions call, never inside the same
     * drain. That keeps the controller thread free of unbounded
     * reentry and gives state code a stable per-tick boundary.
     *
     * @par Ordering with sync calls
     * Sync @ref transition calls issued by the controller thread
     * between two drains take effect immediately and are interleaved
     * naturally with the queued requests that the next drain will
     * apply — both paths funnel through the same @ref transition
     * implementation, so the active state evolves in a single,
     * controller-thread-serialised stream.
     */
    virtual void processQueuedTransitions() = 0;

    // ------ State-bound TaskFlow registry ------

    /**
     * @brief Associates a runnable @ref vigine::TaskFlow with @p state so
     *        the engine pumps that flow's tasks while the FSM is in
     *        @p state.
     *
     * The state machine takes ownership of @p taskFlow. The flow is
     * destroyed when the machine itself is destroyed; there is no
     * removal API in this leaf — registrations are append-only for the
     * machine's lifetime. The state machine never invokes the flow on
     * its own; the engine drives the per-tick advance via the shape
     * documented on @ref vigine::engine::IEngine::run after looking up
     * the active flow through @ref taskFlowFor.
     *
     * Reports @ref Result::Code::Error when @p state is not registered
     * or when @p taskFlow is @c nullptr. Reports
     * @ref Result::Code::Error when @p state already has a TaskFlow
     * bound (one-shot; callers that need to swap a flow rebuild the
     * machine). Ownership is consumed by this call regardless of
     * result: @p taskFlow is taken by value, so on every failure path
     * the @c std::unique_ptr held by the parameter is destroyed at
     * function return — its TaskFlow is released along with it. There
     * is no hand-back to the caller. On success the machine takes the
     * flow into its registry; on failure the flow is destroyed before
     * the call returns.
     *
     * Threading: controller-thread-only once a binding is in place via
     * @ref bindToControllerThread. The registration walks the state
     * topology to confirm @p state is a live id, then takes the
     * registry mutex briefly to insert the entry. The mutex is the
     * same one @ref taskFlowFor takes for its read side, so concurrent
     * lookups on other threads always see a coherent table — but
     * mutations stay on the controller per the locked policy.
     */
    virtual vigine::Result
        addStateTaskFlow(StateId                          state,
                         std::unique_ptr<vigine::TaskFlow> taskFlow) = 0;

    /**
     * @brief Returns the @ref vigine::TaskFlow bound to @p state, or
     *        @c nullptr when no flow has been registered for it.
     *
     * The state machine retains ownership of every registered flow;
     * the returned pointer is non-owning and stays valid until the
     * machine is destroyed. The lookup is the engine's per-tick entry
     * point: it inspects @ref current and asks for the flow bound to
     * that id; a @c nullptr return is the explicit "no work registered
     * for this state" signal that lets the engine fall through to the
     * thread manager pump alone.
     *
     * Two overloads are provided so callers observe a const-correct
     * surface: a non-const machine returns a mutable @ref vigine::TaskFlow
     * pointer (the engine needs that handle to call @c runCurrentTask
     * each tick), while a const machine returns @c const @ref vigine::TaskFlow
     * pointer so a @c const @ref IStateMachine reference cannot be used to
     * mutate the bound flow. Both overloads share the same lookup body
     * and lock policy below.
     *
     * Threading: safe from any thread. Takes the registry mutex
     * briefly for the lookup; concurrent lookups serialise against
     * each other and against @ref addStateTaskFlow.
     */
    [[nodiscard]] virtual vigine::TaskFlow *
        taskFlowFor(StateId state) = 0;

    [[nodiscard]] virtual const vigine::TaskFlow *
        taskFlowFor(StateId state) const = 0;

    IStateMachine(const IStateMachine &)            = delete;
    IStateMachine &operator=(const IStateMachine &) = delete;
    IStateMachine(IStateMachine &&)                 = delete;
    IStateMachine &operator=(IStateMachine &&)      = delete;

  protected:
    IStateMachine() = default;
};

} // namespace vigine::statemachine

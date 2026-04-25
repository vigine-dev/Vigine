#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "vigine/api/messaging/isubscriptiontoken.h"
#include "vigine/result.h"
#include "vigine/api/statemachine/istatemachine.h"
#include "vigine/api/statemachine/routemode.h"
#include "vigine/api/statemachine/stateid.h"

namespace vigine
{
class TaskFlow;
} // namespace vigine

namespace vigine::statemachine
{
// Forward declaration only. The concrete StateTopology type is a
// substrate-primitive specialisation defined under @c src/statemachine
// and is never exposed in the public header tree — see INV-11,
// wrapper encapsulation.
class StateTopology;

/**
 * @brief Stateful abstract base that every concrete state machine
 *        derives from.
 *
 * @ref AbstractStateMachine is level 4 of the wrapper recipe the
 * engine's Level-1 subsystem wrappers follow. It carries the state
 * every concrete state machine shares — a private handle to the
 * internal state topology, the currently active @ref StateId, and the
 * selected @ref RouteMode — and supplies default implementations of
 * every @ref IStateMachine method so a minimal concrete state
 * machine only needs to seal the inheritance chain. The internal
 * state topology specialises the graph substrate and translates
 * between @ref StateId and the substrate's own identifier types
 * inside its implementation.
 *
 * The class carries state, so it follows the project's @c Abstract
 * naming convention rather than the @c I pure-virtual prefix. The
 * base is abstract in the logical sense; its default constructor
 * wires up a fresh internal state topology and auto-provisions the
 * default state per UD-3 so every concrete state machine has a live
 * substrate and a valid @ref current state as soon as it is
 * constructed.
 *
 * Composition, not inheritance:
 *   - @ref AbstractStateMachine HAS-A private
 *     @c std::unique_ptr<StateTopology>. It does @b not inherit from
 *     the substrate primitive at the wrapper level. The internal
 *     state topology is the only place where substrate primitives
 *     enter the state machine stack, and it lives strictly under
 *     @c src/statemachine. This keeps the public header tree free of
 *     substrate types (INV-11) and makes the "a state machine IS NOT
 *     a substrate graph" relationship explicit.
 *
 * Default-state auto-provisioning (UD-3):
 *   - The constructor registers one default state and selects it as
 *     the initial (and current) state. A caller that never registers
 *     its own states still sees a valid @ref current id. A caller
 *     that registers its own states freely overrides the initial
 *     selection with @ref setInitial and the current selection with
 *     @ref transition.
 *   - The default state id is stored as a private member so the
 *     concrete closer can expose it through an internal helper if it
 *     ever needs to (the public API does not surface it separately).
 *
 * Routing (UD-3):
 *   - @ref routeMode defaults to @ref RouteMode::Bubble. The value
 *     is stored as a private member; a later leaf that wires the
 *     machine to the message bus reads the stored value when it
 *     resolves where a given event goes.
 *
 * Strict encapsulation:
 *   - All data members are @c private. Derived state machine classes
 *     reach internal state through @c protected accessors; the
 *     single getter returns a reference to the state topology so
 *     concrete derivatives can extend the default implementation
 *     without re-exporting the substrate on their own public surface.
 *
 * Threading model — single controller thread + async drain (locked policy):
 *   - @ref AbstractStateMachine implements the dual-API transition
 *     contract documented on @ref IStateMachine. There is exactly
 *     ONE controller thread per machine, installed once via
 *     @ref bindToControllerThread; the binding is one-shot and a
 *     second attempt fails the compare-exchange against the
 *     default-constructed @c std::thread::id sentinel (Debug fires
 *     the contract assert; Release silently keeps the original).
 *   - The @b synchronous mutator surface — @ref setInitial,
 *     @ref transition, @ref addState, @ref addChildState,
 *     @ref setRouteMode — and the drain pump
 *     @ref processQueuedTransitions are gated through
 *     @ref checkThreadAffinity once the controller binding is in
 *     place. Stray callers from another thread fire an @c assert in
 *     Debug builds and are undefined behaviour in Release. The
 *     binding is opt-in per the contract on
 *     @ref bindToControllerThread: a consumer that calls it before
 *     the first sync mutation gets the affinity gate; a consumer
 *     that never calls it stays in the unbound mode for the life of
 *     the machine and the gate stays inactive.
 *   - The @b asynchronous request surface — @ref requestTransition —
 *     is the only path open to non-controller threads. It posts the
 *     target onto an internal FIFO queue and returns immediately.
 *     Multiple producers may post concurrently; pushes are serialised
 *     under the queue's synchronisation primitive and FIFO order
 *     matches the order in which those pushes were admitted.
 *   - The drain pump @ref processQueuedTransitions is the
 *     controller-thread end of the async path. It takes ONE snapshot
 *     of the queue per call (atomic detach of the live queue under
 *     the queue's synchronisation primitive) and walks the snapshot
 *     outside the lock, delegating each entry to the synchronous
 *     @ref transition call site. Requests posted from inside an
 *     @c onEnter / @c onExit hook or a listener during a drain land
 *     on the @b live queue and are applied on the @b next drain —
 *     never inside the same drain — so the controller thread is
 *     free of unbounded reentry.
 *   - The owner of the controller-thread loop is responsible for
 *     calling @ref processQueuedTransitions on a periodic cadence
 *     while the machine is live and once at teardown (best-effort).
 *     Tests and ad-hoc embedders that own the controller loop call
 *     it directly when they need a deterministic pump point.
 *   - Read-only queries (@ref hasState, @ref parent,
 *     @ref isAncestorOf, @ref current, @ref routeMode,
 *     @ref controllerThread) are safe from any thread and either
 *     take a shared lock on the topology or an atomic load on the
 *     wrapper-side cache. The wrapper layer adds no further
 *     synchronisation beyond the controller-thread gate, the queue
 *     mutex, and the listener-registry mutex documented on
 *     @ref addInvalidationListener.
 */
class AbstractStateMachine : public IStateMachine
{
  public:
    ~AbstractStateMachine() override;

    // ------ IStateMachine: state registration ------

    [[nodiscard]] StateId addState() override;
    [[nodiscard]] bool    hasState(StateId state) const noexcept override;

    // ------ IStateMachine: hierarchy ------

    Result                addChildState(StateId parent, StateId child) override;
    [[nodiscard]] StateId parent(StateId state) const override;
    [[nodiscard]] bool    isAncestorOf(StateId ancestor, StateId descendant) const override;

    // ------ IStateMachine: initial / current ------

    Result                setInitial(StateId state) override;
    Result                transition(StateId state) override;
    [[nodiscard]] StateId current() const noexcept override;

    // ------ IStateMachine: routing ------

    [[nodiscard]] RouteMode routeMode() const noexcept override;
    void                    setRouteMode(RouteMode mode) noexcept override;

    // ------ IStateMachine: thread affinity ------

    void                          bindToControllerThread(std::thread::id controllerId) override;
    [[nodiscard]] std::thread::id controllerThread() const noexcept override;

    // ------ IStateMachine: asynchronous transition request ------

    void requestTransition(StateId target) override;
    void processQueuedTransitions() override;

    // ------ IStateMachine: state-bound TaskFlow registry ------

    vigine::Result
        addStateTaskFlow(StateId                          state,
                         std::unique_ptr<vigine::TaskFlow> taskFlow) override;

    [[nodiscard]] vigine::TaskFlow *taskFlowFor(StateId state) const override;

    // ------ State-invalidation listener registry ------

    /**
     * @brief Registers @p listener so it observes every successful
     *        non-noop transition.
     *
     * The state machine fires @p listener once per non-noop, valid
     * transition, with the @ref StateId of the state that has just been
     * vacated (i.e. the state @ref current returned immediately before
     * the new state took effect). Listeners always fire BEFORE the
     * machine flips the active state, so observers see the OLD state
     * id consistently.
     *
     * No-op transitions (target equal to the current state) and
     * transitions rejected because the target is not registered DO NOT
     * fire any listener — the machine only notifies on genuine state
     * changes that actually propagate.
     *
     * Listeners run synchronously on the controller thread that
     * executed @ref transition or @ref processQueuedTransitions. They
     * must be cheap and non-blocking; the FSM holds no listener mutex
     * across a listener invocation, so a listener may itself call
     * @ref requestTransition or @ref addInvalidationListener safely
     * (the new entry will sit on the registry for the NEXT firing).
     *
     * The returned token follows the project's RAII subscription
     * convention: dropping it (or calling @c cancel) removes the
     * listener from the registry without racing in-flight firings.
     * Returns an inert token (whose @c active is false from the start)
     * when @p listener is empty.
     *
     * Thread-safety:
     *   - The registry mutex serialises register / cancel calls
     *     against each other and against the snapshot copy taken by
     *     the firing path, so register / cancel observe a coherent
     *     vector even when they race the firing path.
     *   - The firing path takes a snapshot of the active listeners
     *     under the registry mutex, then walks the snapshot OUTSIDE
     *     the lock so a listener that re-enters the FSM (e.g.
     *     @ref requestTransition or @ref addInvalidationListener)
     *     does not deadlock on the registry mutex.
     *   - The snapshot semantics imply a deliberate trade-off for
     *     concurrent register / fire pairs: a listener that finishes
     *     registering BEFORE @ref fireInvalidationListeners takes its
     *     snapshot is fully part of the firing; a listener that
     *     finishes registering AFTER the snapshot is taken (but
     *     before the firing path returns) is NOT part of that
     *     firing and will only observe future transitions. From an
     *     observer's perspective, "the registration happened-before
     *     the snapshot" is the contract; out-of-order registrations
     *     are treated as if they occurred after the transition. This
     *     keeps callback bodies free of the registry mutex (no
     *     deadlock on FSM re-entry) and is the same dispatch shape
     *     @ref vigine::messaging::IMessageBus uses.
     *
     * Lifetime invariant: the subscription token returned here MUST
     * NOT outlive the @ref AbstractStateMachine that issued it. The
     * token holds a raw, non-owning back-pointer to its owner and
     * dereferences it from @c cancel; if the owning state machine
     * were destroyed first, @c cancel would dereference a freed
     * object (use-after-free). The engine's strict construction order
     * arranges this naturally: the state machine outlives every
     * engine token that subscribes to it, and engine tokens own
     * their listener subscriptions. A debug-only counter on the
     * state machine asserts the invariant at teardown so wiring
     * mistakes surface immediately under Debug builds. A
     * control-block-based design (mirroring
     * @ref vigine::messaging::IBusControlBlock) would relax the
     * invariant at the cost of an additional shared-state allocation
     * per token; that trade-off is explicitly out of scope for the
     * concrete EngineToken leaf.
     */
    [[nodiscard]] std::unique_ptr<vigine::messaging::ISubscriptionToken>
        addInvalidationListener(std::function<void(StateId)> listener);

    AbstractStateMachine(const AbstractStateMachine &)            = delete;
    AbstractStateMachine &operator=(const AbstractStateMachine &) = delete;
    AbstractStateMachine(AbstractStateMachine &&)                 = delete;
    AbstractStateMachine &operator=(AbstractStateMachine &&)      = delete;

  protected:
    AbstractStateMachine();

    /**
     * @brief Returns a mutable reference to the internal state
     *        topology.
     *
     * Exposed as @c protected so that follow-up concrete state
     * machine classes can add their own specialised cache or
     * traversal on top of the default implementation without
     * re-exporting the substrate on the public surface. The
     * reference is stable for the lifetime of the
     * @ref AbstractStateMachine instance.
     */
    [[nodiscard]] StateTopology       &topology() noexcept;
    [[nodiscard]] const StateTopology &topology() const noexcept;

    /**
     * @brief Returns the id of the default state registered during
     *        construction.
     *
     * Exposed as @c protected so derived closers that want to query
     * or expose the default state (e.g. for diagnostics) can reach
     * it without walking the topology themselves. The public API
     * does not surface the default id separately because callers
     * who register their own states use @ref setInitial /
     * @ref transition instead.
     */
    [[nodiscard]] StateId defaultState() const noexcept;

    /**
     * @brief Debug-only thread-affinity gate for sync mutators.
     *
     * Reads the controller binding installed through
     * @ref bindToControllerThread. When a binding is present and the
     * current thread is not the controller, the helper fires an
     * @c assert in Debug builds. Release builds compile the body to a
     * no-op — the assert is wrapped in @c \#ifndef NDEBUG so the helper
     * disappears entirely with optimisation on. When no binding has
     * been installed yet the helper is a no-op too, matching the
     * "un-bound = assert inactive" contract documented on
     * @ref bindToControllerThread.
     *
     * @c noexcept so it stays safe to call from @c noexcept mutators
     * if any arrive in the future; today every call-site is a
     * plain mutator, but the gate must never throw by design.
     */
    void checkThreadAffinity() const noexcept;

  private:
    /**
     * @brief Owns the internal state topology.
     *
     * The topology is a substrate-primitive specialisation defined
     * under @c src/statemachine; forward-declaring it here keeps the
     * substrate out of the public header tree. Held through a
     * @c std::unique_ptr so the topology's full definition does not
     * have to leak through this header.
     */
    std::unique_ptr<StateTopology> _topology;

    /**
     * @brief Id of the default state registered during construction.
     *
     * Stored so the base can report it through @ref defaultState
     * without re-querying the topology. Never invalid after
     * construction completes; cleared only when the machine is
     * destroyed.
     */
    StateId _defaultState{};

    /**
     * @brief Id of the currently active state.
     *
     * Initialised to @ref _defaultState during construction so the
     * machine has a valid @ref current immediately. Updated by
     * @ref setInitial and @ref transition.
     *
     * Atomic because the header advertises concurrent `current()`
     * reads alongside lifecycle transitions. `StateId` is an 8-byte
     * trivially-copyable pair; `std::atomic<StateId>` is lock-free
     * on every supported 64-bit target.
     */
    std::atomic<StateId> _current{};

    /**
     * @brief Selected hierarchical routing mode.
     *
     * Defaults to @ref RouteMode::Bubble. Updated by
     * @ref setRouteMode; read by @ref routeMode. Atomic for the same
     * reason as @c _current — a single-byte enum, always lock-free.
     */
    std::atomic<RouteMode> _routeMode{RouteMode::Bubble};

    /**
     * @brief Controller thread binding (one-shot, default-constructed
     *        sentinel means @e unbound).
     *
     * Installed once through @ref bindToControllerThread via a
     * compare-exchange on the default-constructed @c std::thread::id
     * sentinel, so a second bind attempt observes a non-sentinel
     * expected value and fails — Debug fires the contract assert,
     * Release keeps the original binding silently. Read back with
     * acquire semantics so the debug gate in @ref checkThreadAffinity
     * and the public @ref controllerThread getter both observe the
     * latest published value.
     *
     * Atomic because the binding is installed by the controller
     * thread but the @ref controllerThread getter is documented as
     * thread-safe; @c std::atomic<std::thread::id> is lock-free on
     * every supported target.
     */
    std::atomic<std::thread::id> _controllerThreadId{};

    /**
     * @brief Mutex serialising pushes to and the controller-thread
     *        snapshot-swap of @c _transitionQueue.
     *
     * Held only for the duration of a single @c push_back or a single
     * @c swap with a stack-local empty deque, so contention is bounded
     * to a few instructions per producer call. Plain (non-@c mutable)
     * because every locking call site — @ref requestTransition and
     * @ref processQueuedTransitions — is non-@c const; the mutex never
     * needs to be taken from a @c const member, so the @c mutable
     * qualifier would be unused.
     */
    std::mutex _queueMutex;

    /**
     * @brief FIFO of pending transition targets posted via
     *        @ref requestTransition.
     *
     * Drained on the controller thread by
     * @ref processQueuedTransitions. The drain takes a single snapshot
     * by @c std::deque::swap with a fresh empty deque under
     * @c _queueMutex, then walks the snapshot outside the lock so
     * @c onEnter / @c onExit handlers fired from inside @ref transition
     * may post follow-up requests without deadlocking. Those follow-up
     * requests sit on the live queue and are processed on the @b next
     * @ref processQueuedTransitions call, per the single-pass contract
     * documented on @ref IStateMachine::processQueuedTransitions.
     */
    std::deque<StateId> _transitionQueue;

    /**
     * @brief Slot in the invalidation-listener registry.
     *
     * Each successful @ref addInvalidationListener call fills one slot.
     * Slots are append-only; @c id is the per-machine monotonic key
     * the matching subscription token carries so a token cancel can
     * find the slot back without iterating by callback identity.
     * @c callback is the listener body; an empty @c callback marks the
     * slot as cancelled and is skipped by the firing path.
     */
    struct InvalidationListenerSlot
    {
        std::uint32_t                id{0};
        std::function<void(StateId)> callback;
    };

    /**
     * @brief Registry of invalidation listeners.
     *
     * Stored as @c std::vector to keep the firing path linear and
     * cache friendly. Cancellations clear the @c callback in place
     * instead of shifting elements so live registrations and the
     * firing-path snapshot iterator never invalidate.
     *
     * @ref addInvalidationListener walks the registry on every new
     * registration looking for a cancelled (empty) slot it can
     * repopulate before appending a new entry. Slot reuse keeps the
     * vector's size bounded by the live-listener count even in
     * processes that churn through many transient subscriptions
     * (e.g. a long-running engine that issues a fresh engine token on
     * every state transition). The bookkeeping is O(slot count) per
     * register / cancel call, which in practice is dominated by the
     * controller's own work since the listener count is small.
     */
    std::vector<InvalidationListenerSlot> _invalidationListeners;

    /**
     * @brief Count of live @ref InvalidationSubscriptionToken handles
     *        addressing this state machine's registry.
     *
     * Bumped by each @ref InvalidationSubscriptionToken constructor
     * (when the registration is non-inert) and decremented by
     * whichever of @ref InvalidationSubscriptionToken::cancel or its
     * destructor first observes the active registration. Used by the
     * @ref AbstractStateMachine destructor in Debug builds to assert
     * the lifetime-ordering invariant documented on
     * @ref addInvalidationListener: no subscription token is allowed
     * to outlive the state machine that issued it. A non-zero count
     * at @ref AbstractStateMachine teardown surfaces a wiring bug
     * immediately under Debug; Release skips the check to keep
     * teardown cost zero.
     */
    std::atomic<std::uint32_t> _liveInvalidationTokens{0};

    /**
     * @brief Mutex serialising listener register / cancel against each
     *        other and against the firing path.
     *
     * Held only briefly for vector mutation. The firing path takes
     * a snapshot of the active slots under the lock and then invokes
     * each listener outside the lock so a listener that itself calls
     * back into the FSM does not deadlock.
     */
    mutable std::mutex _invalidationListenersMutex;

    /**
     * @brief Monotonic id allocator for the registry.
     *
     * Bumped under @c _invalidationListenersMutex on each successful
     * register call. Atomic so the subscription token can read the id
     * back without contending the registry mutex on the cancel path.
     */
    std::atomic<std::uint32_t> _nextInvalidationListenerId{1};

    /**
     * @brief Hash specialisation for @ref StateId so the per-state
     *        TaskFlow registry can use @c std::unordered_map.
     *
     * StateId is an 8-byte trivially-copyable pair of @c std::uint32_t
     * fields; the hasher splices the index and generation into a single
     * 64-bit value before delegating to the standard library's
     * @c std::hash<std::uint64_t>. Declaring the hasher inside the
     * private section keeps the symbol scoped to the registry's storage
     * type — no namespace-level @c std::hash specialisation leaks out
     * through the public header tree.
     */
    struct StateIdHasher
    {
        [[nodiscard]] std::size_t operator()(const StateId &state) const noexcept
        {
            const std::uint64_t blended =
                (static_cast<std::uint64_t>(state.generation) << 32u)
                | static_cast<std::uint64_t>(state.index);
            return std::hash<std::uint64_t>{}(blended);
        }
    };

    /**
     * @brief State-bound TaskFlow registry.
     *
     * One entry per state: the engine looks the @ref vigine::TaskFlow
     * up by @ref StateId on every tick of @c run() and drives
     * @c runCurrentTask while the flow has tasks left to run. The
     * machine owns each registered flow through a @c std::unique_ptr;
     * destroying the machine destroys every flow. Slots are
     * append-only in this leaf — there is no removal API yet, matching
     * the rest of the wrapper surface (states themselves are
     * append-only).
     *
     * Storage shape is a heterogeneous-key @c std::unordered_map keyed
     * by @ref StateId via the @ref StateIdHasher above. The map is
     * guarded by @ref _stateTaskFlowsMutex which the public
     * @ref addStateTaskFlow / @ref taskFlowFor methods take briefly on
     * insert and lookup; the engine holds the looked-up pointer for
     * the duration of one tick after releasing the mutex, which is
     * safe because no removal API races against the read.
     */
    std::unordered_map<StateId, std::unique_ptr<vigine::TaskFlow>, StateIdHasher>
        _stateTaskFlows;

    /**
     * @brief Mutex serialising mutators / lookups on
     *        @ref _stateTaskFlows.
     *
     * Held only briefly: insert is a single map @c emplace, and
     * lookup is a single @c find. The mutex is @c mutable because
     * @ref taskFlowFor is @c const and still needs to take the
     * lock for its read-side protection against a concurrent
     * registration on the controller thread.
     */
    mutable std::mutex _stateTaskFlowsMutex;

    /**
     * @brief Internal helper that fires every active listener with
     *        @p oldState as the argument.
     *
     * Called on the controller thread from @ref transition right
     * before @c _current is mutated. The helper takes a snapshot of
     * the listener vector under @c _invalidationListenersMutex,
     * releases the lock, and then walks the snapshot, invoking each
     * non-empty callback exactly once. Listeners that throw propagate
     * the exception to the caller of @ref transition; the FSM does
     * not swallow listener-side errors because doing so would silently
     * mask programming mistakes in the engine-token subscription path.
     */
    void fireInvalidationListeners(StateId oldState);

    /**
     * @brief Removes the listener slot whose id matches @p id.
     *
     * Idempotent: a second call with the same id is a no-op. Called by
     * the subscription token's @c cancel and by its destructor.
     */
    void cancelInvalidationListener(std::uint32_t id) noexcept;

    /**
     * @brief Subscription token returned by @ref addInvalidationListener.
     *
     * Holds a non-owning reference to the owning state machine and the
     * monotonic id of its registry slot. Dropping the token cancels the
     * slot through @ref cancelInvalidationListener. Idempotent: repeated
     * @c cancel calls are no-ops.
     */
    class InvalidationSubscriptionToken final
        : public vigine::messaging::ISubscriptionToken
    {
      public:
        InvalidationSubscriptionToken(AbstractStateMachine *owner,
                                      std::uint32_t         id) noexcept;
        ~InvalidationSubscriptionToken() override;

        void               cancel() noexcept override;
        [[nodiscard]] bool active() const noexcept override;

        InvalidationSubscriptionToken(const InvalidationSubscriptionToken &)            = delete;
        InvalidationSubscriptionToken &operator=(const InvalidationSubscriptionToken &) = delete;
        InvalidationSubscriptionToken(InvalidationSubscriptionToken &&)                 = delete;
        InvalidationSubscriptionToken &operator=(InvalidationSubscriptionToken &&)      = delete;

      private:
        AbstractStateMachine     *_owner;
        std::atomic<std::uint32_t> _id;
    };
};

} // namespace vigine::statemachine

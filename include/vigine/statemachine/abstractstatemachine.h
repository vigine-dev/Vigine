#pragma once

#include <atomic>
#include <cassert>
#include <memory>
#include <thread>

#include "vigine/result.h"
#include "vigine/statemachine/istatemachine.h"
#include "vigine/statemachine/routemode.h"
#include "vigine/statemachine/stateid.h"

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
 * Thread-safety: the base inherits the state topology's thread-safety
 * policy (reader-writer mutex on the substrate primitive). Callers
 * may query and mutate concurrently; each mutation takes the
 * exclusive lock while each query takes a shared lock. The wrapper
 * layer does not add further synchronisation — every state-machine
 * access path funnels through the topology.
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
};

} // namespace vigine::statemachine

#include "vigine/statemachine/abstractstatemachine.h"

#include <memory>

#include "statemachine/statetopology.h"
#include "vigine/result.h"
#include "vigine/statemachine/routemode.h"
#include "vigine/statemachine/stateid.h"

namespace vigine::statemachine
{

// ---------------------------------------------------------------------------
// Construction / destruction.
//
// The constructor wires up the internal topology, auto-provisions the
// default state per UD-3, and selects it as both the default and the
// current state so @ref current immediately returns a valid id even when
// the caller never registers any state of their own.
// ---------------------------------------------------------------------------

AbstractStateMachine::AbstractStateMachine()
    : _topology{std::make_unique<StateTopology>()}
{
    _defaultState = _topology->addState();
    _current      = _defaultState;
}

AbstractStateMachine::~AbstractStateMachine() = default;

// ---------------------------------------------------------------------------
// Protected accessors — the derived classes reach the internal topology
// through these so the substrate stays invisible on the wrapper's
// public surface.
// ---------------------------------------------------------------------------

StateTopology &AbstractStateMachine::topology() noexcept
{
    return *_topology;
}

const StateTopology &AbstractStateMachine::topology() const noexcept
{
    return *_topology;
}

StateId AbstractStateMachine::defaultState() const noexcept
{
    return _defaultState;
}

// ---------------------------------------------------------------------------
// IStateMachine: state registration. Each delegation is a one-liner; the
// topology does the substrate translation so the wrapper stays thin.
// ---------------------------------------------------------------------------

StateId AbstractStateMachine::addState()
{
    return _topology->addState();
}

bool AbstractStateMachine::hasState(StateId state) const noexcept
{
    return _topology->hasState(state);
}

// ---------------------------------------------------------------------------
// IStateMachine: hierarchy.
// ---------------------------------------------------------------------------

Result AbstractStateMachine::addChildState(StateId parent, StateId child)
{
    return _topology->addChildEdge(parent, child);
}

StateId AbstractStateMachine::parent(StateId state) const
{
    return _topology->parentOf(state);
}

bool AbstractStateMachine::isAncestorOf(StateId ancestor, StateId descendant) const
{
    return _topology->isAncestorOf(ancestor, descendant);
}

// ---------------------------------------------------------------------------
// IStateMachine: initial / current. The base enforces that the target
// state is registered before it updates @c _current; callers that pass a
// stale id observe an @ref Result::Code::Error and no state change.
// ---------------------------------------------------------------------------

Result AbstractStateMachine::setInitial(StateId state)
{
    if (!_topology->hasState(state))
    {
        return Result(Result::Code::Error, "initial state not registered");
    }
    _current = state;
    return Result();
}

Result AbstractStateMachine::transition(StateId state)
{
    if (!_topology->hasState(state))
    {
        return Result(Result::Code::Error, "transition target not registered");
    }
    _current = state;
    return Result();
}

StateId AbstractStateMachine::current() const noexcept
{
    return _current;
}

// ---------------------------------------------------------------------------
// IStateMachine: routing. UD-3 fixes @ref RouteMode::Bubble as the
// default; the base stores the selection and a later leaf that wires
// the machine to the message bus reads it back.
// ---------------------------------------------------------------------------

RouteMode AbstractStateMachine::routeMode() const noexcept
{
    return _routeMode;
}

void AbstractStateMachine::setRouteMode(RouteMode mode) noexcept
{
    _routeMode = mode;
}

} // namespace vigine::statemachine

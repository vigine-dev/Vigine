#include <vigine/statemachine.h>
#include <algorithm>

namespace vigine {

StateMachine::StateMachine() 
    : _currState(nullptr)
{
}

AbstractState* StateMachine::addState(StateUPtr state)
{
    if (!state) 
        return nullptr;

    // Store the state
    _states.push_back(std::move(state));
    return _states.back().get();
}

bool StateMachine::isStateRegistered(AbstractState* state) const
{
    if (!state)
        return false;

    return std::find_if(_states.begin(), _states.end(),
        [state](const auto& s) { return s.get() == state; }) != _states.end();
}

Result StateMachine::addTransition(AbstractState* from, AbstractState* to, Result::Code resultCode)
{
    if (!from || !to)
        return Result(Result::Code::Error, "Invalid pointer provided for transition");

    if (!isStateRegistered(from))
        return Result(Result::Code::Error, "From state is not registered");

    if (!isStateRegistered(to))
        return Result(Result::Code::Error, "To state is not registered");

    _transitions[from].emplace_back(resultCode, to);

    return Result();
}

void StateMachine::changeStateTo(AbstractState* newState)
{
    if (!newState) 
        return;

    // Update current state
    _currState = newState;
}

AbstractState* StateMachine::currentState() const
{
    return _currState;
}

void StateMachine::runCurrentState()
{
    if (!_currState)
        return;

    // Execute current state
    _currState->enter();
    _currState->update();
    
    // Get execution status from exit
    auto currStatus = _currState->exit();
    
    // Check possible transitions
    auto transitions = _transitions.find(_currState);
    _currState = nullptr;

    if (transitions == _transitions.end())
        return;

    for (const auto& [relStatus, relState] : transitions->second) 
    {
        if (relStatus != currStatus.code())
            continue;

        // Found matching transition
        changeStateTo(relState);
        break;
    }
}

bool StateMachine::hasStatesToRun() const
{
    return _currState != nullptr;
}

} // namespace vigine

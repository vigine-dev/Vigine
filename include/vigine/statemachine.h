#pragma once

#include "abstractstate.h"
#include "result.h"

#include <memory>
#include <vector>
#include <unordered_map>

namespace vigine {

class StateMachine
{
    using StateUPtr = std::unique_ptr<AbstractState>;
    using StateContainer = std::vector<StateUPtr>;
    using Transition = std::pair<Result::Code, AbstractState*>;
    using TransitionContainer = std::vector<Transition>;
    using TransitionMap = std::unordered_map<AbstractState*, TransitionContainer>;

public:
    StateMachine();
    
    // Add a state and return pointer to it
    AbstractState* addState(StateUPtr state);

    // Add a transition between states
    Result addTransition(AbstractState* from, AbstractState* to, Result::Code resultCode);
    
    // Change current state
    void changeStateTo(AbstractState* newState);
    
    // Get current state
    AbstractState* currentState() const;
    
    // Run current state
    void runCurrentState();
    
    // Check if there are states to run
    bool hasStatesToRun() const;

private:
    bool isStateRegistered(AbstractState* state) const;

private:
    StateContainer _states;
    TransitionMap _transitions;
    AbstractState* _currState;
};

} // namespace vigine



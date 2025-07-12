#pragma once

#include "abstractstate.h"
#include "result.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace vigine
{
class Engine;

class StateMachine
{
    using StateUPtr           = std::unique_ptr<AbstractState>;
    using StateContainer      = std::vector<StateUPtr>;
    using Transition          = std::pair<Result::Code, AbstractState *>;
    using TransitionContainer = std::vector<Transition>;
    using TransitionMap       = std::unordered_map<AbstractState *, TransitionContainer>;

  public:
    // Add a state and return pointer to it
    AbstractState *addState(StateUPtr state);

    // Add a transition between states
    Result addTransition(AbstractState *from, AbstractState *to, Result::Code resultCode);

    // Change current state
    void changeStateTo(AbstractState *newState);

    // Get current state
    AbstractState *currentState() const;

    // Run current state
    void runCurrentState();

    // Check if there are states to run
    bool hasStatesToRun() const;

  private:
    StateMachine(Context *context);
    bool isStateRegistered(AbstractState *state) const;

  private:
    StateContainer _states;
    TransitionMap _transitions;
    AbstractState *_currState;
    Context *_context;

    friend class Engine;
};

} // namespace vigine

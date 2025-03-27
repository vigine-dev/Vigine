#include "vigine/vigine.h"
#include "vigine/statemachine.h"
#include <iostream>

namespace vigine {

Engine::Engine()
    : _stateMachine(std::make_unique<StateMachine>())
{
}

void Engine::run()
{
    while (_stateMachine->hasStatesToRun())
        _stateMachine->runCurrentState();
}

StateMachine* Engine::state()
{
    return _stateMachine.get();
}

void exampleFunction() {}

} // namespace vigine

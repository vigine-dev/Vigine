#include "vigine/vigine.h"

#include "vigine/context.h"
#include "vigine/ecs/entitymanager.h"
#include "vigine/statemachine.h"

#include <iostream>

namespace vigine
{

Engine::Engine()
{
    _entityManager.reset(new EntityManager());
    _context.reset(new Context(_entityManager.get()));
    _stateMachine.reset(new StateMachine(_context.get()));
}

Engine::~Engine() {}

void Engine::run()
{
    while (_stateMachine->hasStatesToRun())
        _stateMachine->runCurrentState();
}

StateMachine *Engine::state() { return _stateMachine.get(); }

Context *Engine::context() { return _context.get(); }

void exampleFunction() {}

} // namespace vigine

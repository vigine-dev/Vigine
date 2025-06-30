#include "vigine/vigine.h"

#include "vigine/context.h"
#include "vigine/ecs/entitymanager.h"
#include "vigine/statemachine.h"

#include <iostream>

namespace vigine
{

Engine::Engine() : _stateMachine{std::make_unique<StateMachine>()}
{
    _entityManager.reset(new EntityManager());
    _context.reset(new Context(_entityManager.get()));
    _stateMachine->setContext(_context.get());
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

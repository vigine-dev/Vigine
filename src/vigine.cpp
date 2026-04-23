#include "vigine/vigine.h"

#include "vigine/context.h"
#include "vigine/ecs/entitymanager.h"
#include "vigine/statemachine.h"
#include "vigine/threading/factory.h"
#include "vigine/threading/ithreadmanager.h"

namespace vigine
{

Engine::Engine()
{
    _entityManager.reset(new EntityManager());
    _threadManager = threading::createThreadManager();
    _context.reset(new Context(_entityManager.get(), _threadManager.get()));
    _stateMachine.reset(new StateMachine(_context.get()));
}

Engine::~Engine() = default;

void Engine::run()
{
    while (_stateMachine->hasStatesToRun())
        _stateMachine->runCurrentState();
}

IStateMachine &Engine::state() const { return *_stateMachine; }

IContext &Engine::context() const { return *_context; }

IEntityManager &Engine::entityManager() const { return *_entityManager; }

} // namespace vigine

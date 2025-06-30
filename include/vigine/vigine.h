#pragma once

#include <memory>

namespace vigine
{

class StateMachine;
class Context;
class EntityManager;

class Engine
{
  public:
    Engine();
    ~Engine();
    void run();
    StateMachine *state();
    Context *context();

  private:
    std::unique_ptr<StateMachine> _stateMachine;
    std::unique_ptr<Context> _context;
    std::unique_ptr<EntityManager> _entityManager;
};

} // namespace vigine

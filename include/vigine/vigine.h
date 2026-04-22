#pragma once

#include <memory>
#include <vector>

namespace vigine
{

class StateMachine;
class Context;
class EntityManager;
class IStateMachine;
class IContext;
class IEntityManager;

namespace threading
{
class IThreadManager;
} // namespace threading

namespace messaging
{
class IMessageBus;
} // namespace messaging

/**
 * @brief Top-level engine object (legacy, pre-R.5.1).
 *
 * @note Superseded for new code by @ref vigine::engine::IEngine and
 *       the @ref vigine::engine::createEngine factory (see
 *       @c vigine/engine/). The new engine wraps @ref IContext (the
 *       R.4.5 aggregator) and encodes the AD-5 C8 strict construction
 *       and destruction order end to end, including
 *       @c context->freeze() on @c run() entry and a thread-safe
 *       idempotent @c shutdown(). The legacy @c Engine below is
 *       retained for source compatibility with pre-R.5.1 call sites;
 *       new code should instantiate an @ref vigine::engine::IEngine
 *       through the factory.
 *
 * The public accessors return references to pure-virtual interfaces so
 * that the engine's ownership model (private @c std::unique_ptr) stays
 * decoupled from the surface callers see. The core domain objects
 * (state machine, context, entity manager) keep their unique-ownership
 * shape; only the accessors now return @c IStateMachine / @c IContext /
 * @c IEntityManager references. Messaging and threading fields are
 * forward-declared and lazily constructed by later leaves
 * (plan_09 and onward); they are default-null in this leaf.
 */
class Engine
{
  public:
    Engine();
    ~Engine();

    void run();

    [[nodiscard]] IStateMachine& state() const;
    [[nodiscard]] IContext& context() const;
    [[nodiscard]] IEntityManager& entityManager() const;

  private:
    // Messaging / threading substrate -- introduced here, left null.
    // Their concrete wiring lands in later leaves (plan_09 for the
    // message bus core, plan_23 for the final construction order).
    std::unique_ptr<threading::IThreadManager>            _threadManager;
    std::shared_ptr<messaging::IMessageBus>               _systemBus;
    std::vector<std::shared_ptr<messaging::IMessageBus>>  _userBuses;

    // Core engine state -- unique ownership preserved.
    std::unique_ptr<StateMachine>  _stateMachine;
    std::unique_ptr<Context>       _context;
    std::unique_ptr<EntityManager> _entityManager;
};

} // namespace vigine

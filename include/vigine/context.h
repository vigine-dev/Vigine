#pragma once

#include "vigine/context/icontext.h"
#include "vigine/ecs/abstractsystem.h"
#include <vigine/abstractservice.h>

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace vigine
{

namespace core::threading
{
class IThreadManager;
} // namespace core::threading

enum class Property;
class EntityManager;

using ServiceInstancesContainer = std::vector<std::pair<const Name, AbstractServiceUPtr>>;
using SystemInstancesContainer  = std::vector<std::pair<const SystemId, AbstractSystemUPtr>>;

/**
 * @brief Legacy engine context.
 *
 * Pre-R.4.5 the engine held this class directly through
 * @c std::unique_ptr<Context>; R.4.5 promotes @ref IContext to a
 * full pure-virtual aggregator, so @ref Context now implements the
 * aggregator methods with minimal stubs that report "not available"
 * to any caller. Legacy callers that still route through
 * @c Engine::context() see their existing service / system registry
 * untouched; new callers that want the full aggregator go through
 * @c vigine::context::createContext.
 *
 * A later leaf retires the legacy class entirely once every example
 * migrates to the aggregator contract.
 */
class Context : public IContext
{
  public:
    AbstractService *service(const ServiceId id, const Name name, const Property property);
    AbstractSystem *system(const SystemId id, const SystemName name, const Property property);
    EntityManager *entityManager() const;

    // ------ IContext aggregator stubs (legacy class carries none of
    //        the new substrates; every accessor reports an error or
    //        returns a null handle so the legacy class keeps compiling
    //        after R.4.5 extends the interface) ------

    [[nodiscard]] messaging::IMessageBus &systemBus() override;

    [[nodiscard]] std::shared_ptr<messaging::IMessageBus>
        createMessageBus(const messaging::BusConfig &config) override;

    [[nodiscard]] std::shared_ptr<messaging::IMessageBus>
        messageBus(messaging::BusId id) const override;

    [[nodiscard]] ecs::IECS                   &ecs() override;
    [[nodiscard]] statemachine::IStateMachine &stateMachine() override;
    [[nodiscard]] taskflow::ITaskFlow         &taskFlow() override;

    [[nodiscard]] core::threading::IThreadManager &threadManager() override;

    [[nodiscard]] std::shared_ptr<service::IService>
        service(service::ServiceId id) const override;

    [[nodiscard]] Result
        registerService(std::shared_ptr<service::IService> service) override;

    void              freeze() noexcept override;
    [[nodiscard]] bool isFrozen() const noexcept override;

  private:
    Context(EntityManager *entityManager, core::threading::IThreadManager *threadManager);
    AbstractServiceUPtr createService(const ServiceId &id, const Name &name);
    AbstractSystemUPtr createSystem(const SystemId &id, const SystemName &name);

  private:
    std::unordered_map<ServiceId, ServiceInstancesContainer> _services;
    std::unordered_map<SystemId, SystemInstancesContainer> _systems;
    EntityManager *_entityManager{nullptr};
    // Non-owning pointer into the engine-owned IThreadManager. Set by
    // Engine::Engine before any task-flow wiring runs; surfaced to
    // callers through threadManager() so TaskFlow::signal's non-Any
    // path and any other context-driven scheduling works on the
    // legacy Engine front door.
    core::threading::IThreadManager *_threadManager{nullptr};
    // Atomic because `isFrozen()` is documented as safe from any
    // thread and runs alongside lifecycle transitions. A plain bool
    // here would be a TSAN-observable data race even though the
    // flag is narrow.
    std::atomic<bool> _frozen{false};

    friend class Engine;
};

} // namespace vigine

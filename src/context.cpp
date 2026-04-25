#include "vigine/context.h"

#include "vigine/api/ecs/iecs.h"
#include "vigine/api/engine/iengine_token.h"
#include "vigine/impl/ecs/platform/windowsystem.h"
#if VIGINE_POSTGRESQL
#include "vigine/experimental/ecs/postgresql/impl/postgresqlsystem.h"
#endif
#include "vigine/impl/ecs/graphics/rendersystem.h"
#include "vigine/api/messaging/busconfig.h"
#include "vigine/api/messaging/busid.h"
#include "vigine/api/messaging/imessagebus.h"
#include "vigine/property.h"
#include "vigine/result.h"
#include "vigine/api/service/iservice.h"
#include "vigine/api/statemachine/istatemachine.h"
#include "vigine/api/taskflow/itaskflow.h"
#include "vigine/core/threading/ithreadmanager.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

vigine::AbstractSystem *vigine::Context::system(const SystemId id, const SystemName name,
                                                const Property property)
{
    AbstractSystem *retVal{nullptr};

    switch (property)
    {
    case Property::New: {
        auto system = createSystem(id, name);
        if (!system)
            return retVal;

        retVal = system.get();
        _systems[id].emplace_back(name, std::move(system));
    }

    break;
    case Property::Exist: {
        if (!_systems.contains(id))
            return retVal;

        SystemInstancesContainer &systemContainer = _systems[id];
        auto it = std::find_if(systemContainer.begin(), systemContainer.end(),
                               [&name](const auto &item) { return item.first == name; });

        if (it != systemContainer.end())
            retVal = it->second.get();
    }
    break;
    default:
        break;
    }

    return retVal;
}

vigine::Context::Context(EntityManager            *entityManager,
                         core::threading::IThreadManager *threadManager)
{
    _entityManager  = entityManager;
    _threadManager  = threadManager;
}

// COPILOT_TODO: Додати фабричні гілки для всіх систем, що вже оголошені в API, зокрема Render;
// зараз частина запитів через Context приречена на nullptr.
vigine::AbstractSystemUPtr vigine::Context::createSystem(const SystemId &id, const SystemName &name)
{
    if (id == "Window")
    {
        auto windowSystem = std::make_unique<vigine::ecs::platform::WindowSystem>(name);

        return std::move(windowSystem);
    }

    if (id == "Render")
    {
        auto renderSystem = std::make_unique<vigine::ecs::graphics::RenderSystem>(name);

        return std::move(renderSystem);
    }

#if VIGINE_POSTGRESQL
    if (id == "PostgreSQL")
    {
        auto postgreSQLSystem = std::make_unique<vigine::experimental::ecs::postgresql::PostgreSQLSystem>(name);

        return std::move(postgreSQLSystem);
    }
#endif

    return nullptr;
}

vigine::EntityManager *vigine::Context::entityManager() const { return _entityManager; }

vigine::AbstractService *vigine::Context::service(const ServiceId id, const Name name,
                                                  const Property property)
{
    AbstractService *retVal{nullptr};

    switch (property)
    {
    case Property::New: {
        auto serv = createService(id, name);
        if (!serv)
            return retVal;

        retVal = serv.get();
        _services[id].emplace_back(name, std::move(serv));
    }

    break;
    case Property::Exist: {
        if (!_services.contains(id))
            return retVal;

        ServiceInstancesContainer &servicesContainer = _services[id];
        auto it = std::find_if(servicesContainer.begin(), servicesContainer.end(),
                               [&name](const auto &item) { return item.first == name; });

        if (it != servicesContainer.end())
            retVal = it->second.get();
    }
    break;
    default:
        break;
    }

    return retVal;
}

// Post-#330: PlatformService, GraphicsService, and DatabaseService have
// been migrated off the legacy `vigine::AbstractService` root-namespace
// base onto the modern `vigine::service::AbstractService` (see
// include/vigine/api/service/abstractservice.h). They are no longer
// constructible through this legacy factory; callers that previously
// reached them via `Context::service(id, name, Property::New)` register
// them on the modern aggregator (`vigine::context::AbstractContext`)
// through `registerService` and resolve them through the modern
// generational accessor
// `vigine::IContext::service(vigine::service::ServiceId)` (also
// available on `vigine::context::AbstractContext` and via the
// engine-bound `vigine::engine::IEngineToken::service(...)`) — distinct
// from the deprecated legacy `service(ServiceId, Name, Property)`
// signature defined in this file, where `ServiceId` is the
// root-namespace `std::string` alias rather than the modern
// `vigine::service::ServiceId` generational handle. The legacy
// signature is retained for transitional callers but is scheduled for
// removal once all services migrate.
//
// The factory itself stays in place for any future legacy service that
// has not yet migrated; the three migrated ids fall through to the
// default `nullptr` return so callers see a clean "unknown id" signal
// rather than a dangling cast target.
vigine::AbstractServiceUPtr vigine::Context::createService(const ServiceId & /*id*/,
                                                            const Name & /*name*/)
{
    return nullptr;
}

// ---------------------------------------------------------------------------
// Legacy Context — IContext aggregator stubs.
//
// Post R.4.5 the IContext interface became the full engine aggregator. The
// legacy Context does not own a thread manager, system bus, or Level-1
// wrappers, so these accessors either throw (references must be non-null)
// or return empty handles. Callers that need the real aggregator go through
// vigine::context::createContext.
// ---------------------------------------------------------------------------

vigine::messaging::IMessageBus &vigine::Context::systemBus()
{
    throw std::logic_error{
        "legacy vigine::Context has no system bus; use vigine::context::createContext"};
}

std::shared_ptr<vigine::messaging::IMessageBus>
vigine::Context::createMessageBus(const vigine::messaging::BusConfig & /*config*/)
{
    return nullptr;
}

std::shared_ptr<vigine::messaging::IMessageBus>
vigine::Context::messageBus(vigine::messaging::BusId /*id*/) const
{
    return nullptr;
}

vigine::ecs::IECS &vigine::Context::ecs()
{
    throw std::logic_error{
        "legacy vigine::Context has no Level-1 ECS wrapper; use vigine::context::createContext"};
}

vigine::statemachine::IStateMachine &vigine::Context::stateMachine()
{
    throw std::logic_error{
        "legacy vigine::Context has no Level-1 state machine wrapper; use vigine::context::createContext"};
}

vigine::taskflow::ITaskFlow &vigine::Context::taskFlow()
{
    throw std::logic_error{
        "legacy vigine::Context has no Level-1 task flow wrapper; use vigine::context::createContext"};
}

vigine::core::threading::IThreadManager &vigine::Context::threadManager()
{
    if (_threadManager == nullptr)
    {
        throw std::logic_error{
            "legacy vigine::Context was built without a thread manager; "
            "construct Engine via its default ctor so the thread manager is plumbed through"};
    }
    return *_threadManager;
}

std::shared_ptr<vigine::service::IService>
vigine::Context::service(vigine::service::ServiceId /*id*/) const
{
    return nullptr;
}

vigine::Result
vigine::Context::registerService(std::shared_ptr<vigine::service::IService> /*service*/)
{
    return Result{
        Result::Code::Error,
        "legacy vigine::Context does not accept service registrations; use vigine::context::createContext"};
}

std::unique_ptr<vigine::engine::IEngineToken>
vigine::Context::makeEngineToken(vigine::statemachine::StateId /*boundState*/)
{
    // The legacy aggregator never wired up an IStateMachine, so the
    // engine-token wiring (back-reference to context + FSM listener
    // registration) cannot proceed. Return @c nullptr per the
    // IContext docstring's legacy-stub clause; callers that need a
    // live token construct a context through
    // @c vigine::context::createContext, which routes through the
    // R.4.5 aggregator @ref vigine::context::AbstractContext where
    // the factory is fully wired.
    return nullptr;
}

void vigine::Context::freeze() noexcept
{
    _frozen = true;
}

bool vigine::Context::isFrozen() const noexcept
{
    return _frozen;
}

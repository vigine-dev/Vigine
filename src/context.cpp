#include "vigine/context.h"

#include "vigine/ecs/iecs.h"
#include "vigine/ecs/platform/windowsystem.h"
#if VIGINE_POSTGRESQL
#include "vigine/experimental/ecs/postgresql/impl/postgresqlsystem.h"
#endif
#include "vigine/ecs/render/rendersystem.h"
#include "vigine/messaging/busconfig.h"
#include "vigine/messaging/busid.h"
#include "vigine/messaging/imessagebus.h"
#include "vigine/property.h"
#include "vigine/result.h"
#include "vigine/service/databaseservice.h"
#include "vigine/service/graphicsservice.h"
#include "vigine/api/service/iservice.h"
#include "vigine/service/platformservice.h"
#include "vigine/statemachine/istatemachine.h"
#include "vigine/taskflow/itaskflow.h"
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
        auto windowSystem = std::make_unique<vigine::platform::WindowSystem>(name);

        return std::move(windowSystem);
    }

    if (id == "Render")
    {
        auto renderSystem = std::make_unique<vigine::graphics::RenderSystem>(name);

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

// COPILOT_TODO: Додати створення GraphicsService; зараз service("Graphics", ...) завжди повертає
// nullptr, хоча сервіс присутній у публічному API.
vigine::AbstractServiceUPtr vigine::Context::createService(const ServiceId &id, const Name &name)
{
    if (id == "Platform")
    {
        auto platformService = std::make_unique<vigine::platform::PlatformService>(name);
        platformService->setContext(this);

        return std::move(platformService);
    }

    if (id == "Graphics")
    {
        auto graphicsService = std::make_unique<vigine::graphics::GraphicsService>(name);
        graphicsService->setContext(this);

        return std::move(graphicsService);
    }

    if (id == "Database")
    {
        auto dbServ = std::make_unique<DatabaseService>(name);
        dbServ->setContext(this);

        return std::move(dbServ);
    }

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

void vigine::Context::freeze() noexcept
{
    _frozen = true;
}

bool vigine::Context::isFrozen() const noexcept
{
    return _frozen;
}

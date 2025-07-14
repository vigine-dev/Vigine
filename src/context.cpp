#include "vigine/context.h"

#include "vigine/ecs/postgresql/postgresqlsystem.h"
#include "vigine/property.h"
#include "vigine/service/databaseservice.h"

#include <algorithm>
#include <utility>

vigine::AbstractSystem *vigine::Context::system(const SystemId id, const SystemName name,
                                                const Property property)
{
    AbstractSystem *retVal{nullptr};

    switch (property)
        {
        case Property::New:
            {
                auto system = createSystem(id, name);
                if (!system)
                    return retVal;

                retVal = system.get();
                _systems[id].emplace_back(name, std::move(system));
            }

            break;
        case Property::Exist:
            {
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

vigine::Context::Context(EntityManager *entityManager) { _entityManager = entityManager; }

vigine::AbstractSystemUPtr vigine::Context::createSystem(const SystemId &id, const SystemName &name)
{
    if (id == "PostgreSQL")
        {
            auto postgreSQLSystem = vigine::postgresql::make_PostgreSQLSystemUPtr(name);

            return std::move(postgreSQLSystem);
        }

    return nullptr;
}

vigine::EntityManager *vigine::Context::entityManager() const { return _entityManager; }

vigine::AbstractService *vigine::Context::service(const ServiceId id, const Name name,
                                                  const Property property)
{
    AbstractService *retVal{nullptr};

    switch (property)
        {
        case Property::New:
            {
                auto serv = createService(id, name);
                if (!serv)
                    return retVal;

                retVal = serv.get();
                _services[id].emplace_back(name, std::move(serv));
            }

            break;
        case Property::Exist:
            {
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

vigine::AbstractServiceUPtr vigine::Context::createService(const ServiceId &id, const Name &name)
{
    if (id == "Database")
        {
            auto dbServ = make_DatabaseServiceUPtr(name);
            dbServ->setContext(this);

            return std::move(dbServ);
        }

    return nullptr;
}

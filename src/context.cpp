#include <vigine/context.h>
#include <vigine/property.h>
#include <vigine/service/databaseservice.h>

#include <algorithm>
#include <utility>

vigine::Context::Context(EntityManager *entityManager) { _entityManager = entityManager; }

vigine::EntityManager *vigine::Context::entityManager() const { return _entityManager; }

vigine::AbstractService *vigine::Context::service(const ServiceId id, const ServiceName name,
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

vigine::AbstractServiceUPtr vigine::Context::createService(const ServiceId &id,
                                                           const ServiceName &name)
{

    if (id == "Database")
        {

            auto dbServ = std::make_unique<DatabaseService>(name);
            dbServ->setContext(this);

            return std::move(dbServ);
        }

    return nullptr;
}

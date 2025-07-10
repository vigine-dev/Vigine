#pragma conce

#include "vigine/ecs/abstractsystem.h"
#include <vigine/abstractservice.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace vigine
{

enum class Property;
class EntityManager;

using ServiceInstancesContainer = std::vector<std::pair<const ServiceName, AbstractServiceUPtr>>;
using SystemInstancesContainer  = std::vector<std::pair<const SystemId, AbstractSystemUPtr>>;

class Context
{
  public:
    AbstractService *service(const ServiceId id, const ServiceName name, const Property property);
    AbstractSystem *system(const SystemId id, const SystemName name, const Property property);
    EntityManager *entityManager() const;

  private:
    Context(EntityManager *entityManager);
    AbstractServiceUPtr createService(const ServiceId &id, const ServiceName &name);
    AbstractSystemUPtr createSystem(const SystemId &id, const SystemName &name);

  private:
    std::unordered_map<ServiceId, ServiceInstancesContainer> _services;
    std::unordered_map<SystemId, SystemInstancesContainer> _systems;
    EntityManager *_entityManager{nullptr};

    friend class Engine;
};

} // namespace vigine

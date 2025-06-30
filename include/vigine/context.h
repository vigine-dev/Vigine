#pragma conce

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

class Context
{
  public:
    AbstractService *service(const ServiceId id, const ServiceName name, const Property property);

  private:
    Context(EntityManager *entityManager);
    AbstractServiceUPtr createService(const ServiceId &id, const ServiceName &name);
    EntityManager *entityManager() const;

  private:
    std::unordered_map<ServiceId, ServiceInstancesContainer> _services;
    EntityManager *_entityManager{nullptr};

    friend class Engine;
};

} // namespace vigine

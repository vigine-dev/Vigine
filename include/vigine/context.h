#pragma conce

#include <vigine/abstractservice.h>

#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

namespace vigine {

enum class Property;

using ServiceInstancesContainer =
    std::vector<std::pair<const ServiceName, AbstractServiceUPtr>>;

class Context {
public:
  Context();
  AbstractService *service(const ServiceId id, const ServiceName name,
                           const Property property);

private:
  AbstractServiceUPtr createService(const ServiceId id, const ServiceName name);

private:
  std::unordered_map<ServiceId, ServiceInstancesContainer> _services;
};

} // namespace vigine

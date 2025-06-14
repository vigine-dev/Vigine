#include <vigine/context.h>

#include <vigine/property.h>

#include <algorithm>
#include <utility>

// TODO: move to another place
class CppParser : public vigine::AbstractService {
public:
  using AbstractService::AbstractService;

  ~CppParser() override {}
  vigine::ServiceId id() override { return "CppParser"; }
};

vigine::AbstractService *vigine::Context::service(const ServiceId id,
                                                  const ServiceName name,
                                                  const Property property) {
  AbstractService *retVal{nullptr};

  switch (property) {
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
    auto it =
        std::find_if(servicesContainer.begin(), servicesContainer.end(),
                     [&name](const auto &item) { return item.first == name; });

    if (it != servicesContainer.end())
      retVal = it->second.get();
  } break;
  default:
    break;
  }

  return retVal;
}

vigine::Context::Context() {}

vigine::AbstractServiceUPtr
vigine::Context::createService(const ServiceId id, const ServiceName name) {
  if (id == "CppParser")
    return std::make_unique<CppParser>(name);

  return nullptr;
}

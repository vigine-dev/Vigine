#pragma once

#include <string>
#include <memory>

namespace vigine {

using ServiceId = std::string;
using ServiceName = std::string;

class AbstractService {
public:
  AbstractService(const ServiceName name) : _name{name} {}
  virtual ~AbstractService() {}

  // Each service need to return self id (name of the service like 'Http')
  virtual ServiceId id() = 0;

  // This is instance name (like 'MyCustomService')
  ServiceName name() { return _name; }

private:
  ServiceName _name;
};

using AbstractServiceUPtr = std::unique_ptr<AbstractService>;

} // namespace vigine

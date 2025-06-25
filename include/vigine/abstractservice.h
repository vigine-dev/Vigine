#pragma once

#include <memory>
#include <string>

namespace vigine {

using ServiceId = std::string;
using ServiceName = std::string;

class AbstractService {
public:
  virtual ~AbstractService() {}

  // Each service need to return self id (name of the service like 'Http')
  virtual ServiceId id() const = 0;

  // This is instance name (like 'MyCustomService')
  ServiceName name() { return _name; }

protected:
  AbstractService(const ServiceName& name) : _name{name} {}

private:
  ServiceName _name;
};

using AbstractServiceUPtr = std::unique_ptr<AbstractService>;

} // namespace vigine

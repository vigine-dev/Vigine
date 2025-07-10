#pragma once

#include "contextholder.h"
#include "entitybindinghost.h"

#include <memory>
#include <string>

namespace vigine
{

using ServiceId   = std::string;
using ServiceName = std::string;

class Entity;

class AbstractService : public ContextHolder, public EntityBindingHost
{
  public:
    virtual ~AbstractService();

    // Each service need to return self id (name of the service like 'Http')
    virtual ServiceId id() const = 0;

    // This is instance name (like 'MyCustomService')
    ServiceName name();

  protected:
    AbstractService(const ServiceName &name);

  private:
    ServiceName _name;
};

using AbstractServiceUPtr = std::unique_ptr<AbstractService>;

} // namespace vigine

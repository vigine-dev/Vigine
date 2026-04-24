#pragma once

/**
 * @file abstractservice.h
 * @brief Legacy base class for named services bound to Context and Entity.
 */

#include "contextholder.h"
#include "entitybindinghost.h"

#include "vigine/base/name.h"

#include <memory>
#include <string>

namespace vigine
{

using ServiceId = std::string;

class Entity;

/**
 * @brief Base for services that share a Context and may bind to an Entity.
 *
 * Concrete services identify their kind via id() (e.g. "Http") and
 * carry an instance-level Name supplied at construction. They combine
 * ContextHolder (for global context access) with EntityBindingHost
 * (for optional per-Entity binding).
 */
class AbstractService : public ContextHolder, public EntityBindingHost
{
  public:
    virtual ~AbstractService();

    // Each service need to return self id (name of the service like 'Http')
    [[nodiscard]] virtual ServiceId id() const = 0;

    // This is instance name (like 'MyCustomService')
    [[nodiscard]] Name name();

  protected:
    AbstractService(const Name &name);

  private:
    Name _name;
};

using AbstractServiceUPtr = std::unique_ptr<AbstractService>;

} // namespace vigine

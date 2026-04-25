#pragma once

/**
 * @file abstractservice.h
 * @brief Legacy base class for named services bound to Context and Entity.
 */

#include "vigine/base/name.h"

#include <memory>
#include <string>

namespace vigine
{

using ServiceId = std::string;

class Context;
class Entity;

/**
 * @brief Base for services that share a Context and may bind to an Entity.
 *
 * Concrete services identify their kind via id() (e.g. "Http") and
 * carry an instance-level Name supplied at construction. The class
 * holds a non-owning Context pointer (set externally via setContext)
 * and an optional per-Entity binding (bindEntity / unbindEntity /
 * getBoundEntity). Both responsibilities are now carried by
 * composition (private members); the previous ContextHolder and
 * EntityBindingHost mixins have been deleted.
 */
class AbstractService
{
  public:
    virtual ~AbstractService();

    // Each service need to return self id (name of the service like 'Http')
    [[nodiscard]] virtual ServiceId id() const = 0;

    // This is instance name (like 'MyCustomService')
    [[nodiscard]] Name name();

    void setContext(Context *context);

    void bindEntity(Entity *entity);
    void unbindEntity();
    [[nodiscard]] Entity *getBoundEntity() const;

  protected:
    AbstractService(const Name &name);

    [[nodiscard]] Context *context() const;
    virtual void contextChanged();

    virtual void entityBound();
    virtual void entityUnbound();

  private:
    Name _name;
    Context *_context{nullptr};
    Entity *_entity{nullptr};
};

using AbstractServiceUPtr = std::unique_ptr<AbstractService>;

} // namespace vigine

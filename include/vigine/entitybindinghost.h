#pragma once

namespace vigine
{
class Entity;

class EntityBindingHost
{
  public:
    virtual ~EntityBindingHost();

    void bindEntity(Entity *entity);
    void unbindEntity();
    Entity *getBoundEntity() const;

  protected:
    EntityBindingHost();

    virtual void entityBound();
    virtual void entityUnbound();

  private:
    Entity *_entity{nullptr};
};
} // namespace vigine

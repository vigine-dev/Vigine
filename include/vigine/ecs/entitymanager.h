#pragma once

#include <memory>
#include <vector>

namespace vigine
{
class Engine;
class Entity;

using EntityUPtr = std::unique_ptr<Entity>;

class EntityManager
{
  public:
    ~EntityManager();
    Entity *createEntity();
    void removeEntity(Entity *entity);

  private:
    EntityManager();
    std::vector<EntityUPtr> _entities;

    friend class Engine;
};
} // namespace vigine

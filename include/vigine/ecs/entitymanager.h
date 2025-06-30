#pragma once

#include <memory>
#include <vector>

namespace vigine
{
class Entity;

using EntityUPtr = std::unique_ptr<Entity>;

class EntityManager
{
  public:
    Entity *createEntity();
    void removeEntity(Entity *entity);

  private:
    EntityManager();
    std::vector<EntityUPtr> _entities;

    friend class Vigine;
};
} // namespace vigine

#pragma once

#include <map>
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
    void addAlias(Entity *entity, const std::string &alias);
    Entity *getEntityByAlias(const std::string &alias) const;

  private:
    EntityManager();
    std::vector<EntityUPtr> _entities;
    std::map<std::string, Entity *> _entityAliases;

    friend class Engine;
};
} // namespace vigine

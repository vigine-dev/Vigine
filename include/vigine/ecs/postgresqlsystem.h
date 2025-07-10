#pragma once

#include "vigine/ecs/abstractsystem.h"

#include <unordered_map>

namespace vigine
{
struct PostgreSQLSystemComponents
{
};

using PostgreSQLSystemComponentsUPtr = std::unique_ptr<PostgreSQLSystemComponents>;

class PostgreSQLSystem : public AbstractSystem
{
  public:
    PostgreSQLSystem(const SystemName &name);
    ~PostgreSQLSystem() override;

    SystemId id() const override;

    bool hasComponents(Entity *entity) const override;
    void createComponents(Entity *entity) override;
    void destroyComponents(Entity *entity) override;

  private:
    std::unordered_map<Entity *, PostgreSQLSystemComponentsUPtr> _entityComponents;
};
}; // namespace vigine

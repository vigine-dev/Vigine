#pragma once

#include "vigine/ecs/abstractsystem.h"

namespace vigine
{
class PostgreSQLSystem : public AbstractSystem
{
  public:
    PostgreSQLSystem(const SystemName &name);
    ~PostgreSQLSystem() override;

    SystemId id() const override;
};
}; // namespace vigine

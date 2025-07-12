#pragma once

#include "vigine/result.h"
#include <vigine/abstractservice.h>
#include <vigine/ecs/entity.h>

namespace vigine
{

class PostgreSQLSystem;

class DatabaseService : public AbstractService
{
  public:
    DatabaseService(const ServiceName &name);

    void contextChanged() override;

    ServiceId id() const override;
    Result connectToDb();

  protected:
    void entityBound() override;

  private:
    PostgreSQLSystem *_postgressSystem{nullptr};
};
} // namespace vigine

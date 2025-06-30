#pragma once

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
    Entity connectToDb();

  private:
    PostgreSQLSystem *_postgress{nullptr};
};
} // namespace vigine

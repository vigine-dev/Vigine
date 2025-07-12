#pragma once

#include "vigine/ecs/abstractsystem.h"

#include <pqxx/pqxx>
#include <unordered_map>

namespace vigine
{
class PostgreSQLComponent;

using PostgreSQLSystemComponentUPtr = std::unique_ptr<PostgreSQLComponent>;

struct ConnectionData
{
    std::string host;
    std::string port;
    std::string dbName;
    std::string dbUserName;
    std::string password;
};

class PostgreSQLSystem : public AbstractSystem
{
  public:
    PostgreSQLSystem(const SystemName &name);
    ~PostgreSQLSystem() override;

    SystemId id() const override;

    // interface implementation
    bool hasComponents(Entity *entity) const override;
    void createComponents(Entity *entity) override;
    void destroyComponents(Entity *entity) override;

    // custom
    void setConnectionData(const ConnectionData &connectionData);
    void connect();
    pqxx::result select(const std::string &query);

  protected:
    virtual void entityBound();
    virtual void entityUnbound();

  private:
    std::unordered_map<Entity *, PostgreSQLSystemComponentUPtr> _entityComponents;
    PostgreSQLComponent *_boundEntityComponent;
};
}; // namespace vigine

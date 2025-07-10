#pragma once

#include "vigine/entitybindinghost.h"

#include <memory>
#include <string>

namespace vigine
{
using SystemId   = std::string;
using SystemName = std::string;

class AbstractSystem : public EntityBindingHost
{
  public:
    ~AbstractSystem() override;

    virtual SystemId id() const = 0;
    SystemName name();

  protected:
    AbstractSystem(const SystemName &name);

  private:
    SystemName _name;
};

using AbstractSystemUPtr = std::unique_ptr<AbstractSystem>;

} // namespace vigine

#pragma once

#include "vigine/entitybindinghost.h"

namespace vigine
{

class AbstractSystem : public EntityBindingHost
{
  public:
    virtual ~AbstractSystem(){};

  protected:
    AbstractSystem() = default;
};

} // namespace vigine

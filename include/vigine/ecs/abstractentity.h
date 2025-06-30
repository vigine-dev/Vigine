#pragma once

#include "abstractcomponent.h"

#include <string>
#include <vector>

namespace vigine
{
class AbstractEntity
{
  public:
    virtual ~AbstractEntity() {};

  protected:
    AbstractEntity() = default;
};

} // namespace vigine

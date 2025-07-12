#pragma once

#include "contextholder.h"
#include "result.h"

namespace vigine
{

class Context;

class AbstractTask : public ContextHolder
{
  public:
    virtual ~AbstractTask();
    virtual Result execute() = 0;

  protected:
    AbstractTask();
};

} // namespace vigine

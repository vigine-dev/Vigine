#pragma once

/**
 * @file abstracttask.h
 * @brief Legacy base class for a single unit of work run inside a TaskFlow.
 */

#include "contextholder.h"
#include "result.h"

namespace vigine
{

class Context;

/**
 * @brief Base for task objects executed by TaskFlow / StateMachine.
 *
 * A task receives its Context via ContextHolder, runs once when its
 * owning flow schedules it, and returns a Result whose Code drives the
 * next transition. Concrete tasks implement execute().
 */
class AbstractTask : public ContextHolder
{
  public:
    virtual ~AbstractTask();
    [[nodiscard]] virtual Result execute() = 0;

  protected:
    AbstractTask();
};

} // namespace vigine

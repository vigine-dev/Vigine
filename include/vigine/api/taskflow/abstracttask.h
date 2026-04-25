#pragma once

/**
 * @file abstracttask.h
 * @brief Legacy base class for a single unit of work run inside a TaskFlow.
 */

#include "vigine/result.h"

namespace vigine
{

class Context;

/**
 * @brief Base for task objects executed by TaskFlow / StateMachine.
 *
 * A task carries a non-owning Context pointer set externally via
 * setContext, runs once when its owning flow schedules it, and returns
 * a Result whose Code drives the next transition. Concrete tasks
 * implement execute() and may override contextChanged() to react to
 * context binding. The Context pointer is held by composition (private
 * member); the previous ContextHolder mixin has been deleted.
 */
class AbstractTask
{
  public:
    virtual ~AbstractTask();
    [[nodiscard]] virtual Result execute() = 0;

    void setContext(Context *context);

  protected:
    AbstractTask();

    Context *context() const;
    virtual void contextChanged();

  private:
    Context *_context{nullptr};
};

} // namespace vigine

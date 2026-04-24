#pragma once

/**
 * @file abstractstate.h
 * @brief Legacy base class for a state hosted by StateMachine.
 */

#include "result.h"
#include "taskflow.h"

#include <memory>

namespace vigine
{

class Context;

/**
 * @brief Base for state objects driven by StateMachine.
 *
 * A state owns a TaskFlow which is executed between enter() and exit().
 * The Result returned by exit() is forwarded to StateMachine to pick
 * the next transition. Concrete states implement enter() and exit().
 */
class AbstractState // ENCAP EXEMPTION: legacy; protected _taskFlow/_isActive/_context pending cleanup
{
  public:
    virtual ~AbstractState() = default;

    // Main state execution method
    // COPILOT_TODO: Додати перевірку _context перед enter()/exit(), інакше стан може виконуватися
    // без валідного Context.
    [[nodiscard]] Result operator()()
    {
        if (!_taskFlow)
            return Result(Result::Code::Error, "Task flow needed");

        enter();

        (*_taskFlow)();

        return exit();
    }

    // Task flow management methods
    void setTaskFlow(std::unique_ptr<TaskFlow> taskFlow)
    {
        if (!taskFlow)
            return;

        _taskFlow = std::move(taskFlow);
        _taskFlow->setContext(_context);
    }

    [[nodiscard]] TaskFlow *getTaskFlow() const { return _taskFlow.get(); }

    void setContext(Context *context)
    {
        _context = context;

        if (_taskFlow)
            _taskFlow->setContext(_context);
    }

  protected:
    AbstractState() = default;

    // State lifecycle methods
    virtual void enter()                = 0;
    [[nodiscard]] virtual Result exit() = 0;

    Context *context() { return _context; }

  protected:
    std::unique_ptr<TaskFlow> _taskFlow;
    bool _isActive = true;
    Context *_context{nullptr};
};

} // namespace vigine

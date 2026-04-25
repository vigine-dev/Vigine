#pragma once

/**
 * @file abstractstate.h
 * @brief Legacy base class for a state hosted by StateMachine.
 */

#include "vigine/impl/taskflow/taskflow.h"
#include "vigine/result.h"

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
        // Propagate the state's context binding only when one is
        // installed. The state may receive its task flow before its
        // own setContext is called by the state machine; in that case
        // the task flow stays unbound until the state's setContext
        // forwards the binding below.
        if (_context != nullptr)
            _taskFlow->setContext(*_context);
    }

    [[nodiscard]] TaskFlow *getTaskFlow() const { return _taskFlow.get(); }

    void setContext(Context &context)
    {
        _context = &context;

        if (_taskFlow)
            _taskFlow->setContext(context);
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

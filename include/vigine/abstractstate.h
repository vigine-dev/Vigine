#pragma once

#include "result.h"
#include "taskflow.h"

#include <memory>

namespace vigine
{

class Context;

class AbstractState
{
  public:
    virtual ~AbstractState() = default;

    // Main state execution method
    Result operator()()
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

    TaskFlow *getTaskFlow() const { return _taskFlow.get(); }

    void setContext(Context *context)
    {
        _context = context;

        if (_taskFlow)
            _taskFlow->setContext(_context);
    }

  protected:
    AbstractState() = default;

    // State lifecycle methods
    virtual void enter()  = 0;
    virtual Result exit() = 0;

    Context *context() { return _context; }

  protected:
    std::unique_ptr<TaskFlow> _taskFlow;
    bool _isActive = true;
    Context *_context{nullptr};
};

} // namespace vigine

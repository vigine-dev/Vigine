#pragma once

#include "taskflow.h"
#include "result.h"

#include <memory>

namespace vigine {

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
    void setTaskFlow(std::unique_ptr<TaskFlow> taskFlow) {
        _taskFlow = std::move(taskFlow);
    }

    TaskFlow* getTaskFlow() const {
        return _taskFlow.get();
    }

protected:
    AbstractState() = default;

    // State lifecycle methods
    virtual void enter() = 0;
    virtual Result exit() = 0;

protected:
    std::unique_ptr<TaskFlow> _taskFlow;
    bool _isActive = true;
};

} // namespace vigine

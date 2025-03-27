#pragma once

#include "abstracttaskflow.h"
#include "result.h"

#include <memory>

namespace vigine {

class AbstractState
{
public:
    virtual ~AbstractState() = default;
    
    // State lifecycle methods
    virtual void enter() = 0;
    virtual Result exit() = 0;
    virtual void update() = 0;

    // Task flow management methods
    virtual void setTaskFlow(std::unique_ptr<AbstractTaskFlow> taskFlow) {
        _taskFlow = std::move(taskFlow);
    }

    virtual AbstractTaskFlow* getTaskFlow() const {
        return _taskFlow.get();
    }

protected:
    AbstractState() = default;
    std::unique_ptr<AbstractTaskFlow> _taskFlow;
    bool _isActive = true;
};

} // namespace vigine

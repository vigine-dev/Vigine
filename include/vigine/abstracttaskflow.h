#pragma once

#include "abstracttask.h"
#include <vigine/result.h>

#include <memory>
#include <vector>

namespace vigine {

using Task = AbstractTask;
using Relation = std::pair<Result::Code, Task*>;
using RelationContainer = std::vector<Relation>;
using TaskRelation = std::pair<Task*, RelationContainer>;

class AbstractTaskFlow {
public:
    virtual ~AbstractTaskFlow() = default;
    virtual void operator()() = 0;
    virtual void addTask(AbstractTask* task,
                       const RelationContainer &relations) = 0;
    virtual void removeTask(AbstractTask* task) = 0;
    virtual void update() = 0;

protected:
    AbstractTaskFlow() = default;
};

} // namespace vigine

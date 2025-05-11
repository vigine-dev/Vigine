#pragma once

#include "abstracttask.h"
#include "result.h"

#include <memory>
#include <vector>
#include <unordered_map>

namespace vigine {

class TaskFlow {
    using TaskUPtr = std::unique_ptr<AbstractTask>;
    using TaskContainer = std::vector<TaskUPtr>;
    using Transition = std::pair<Result::Code, AbstractTask*>;
    using TransitionContainer = std::vector<Transition>;
    using TransitionMap = std::unordered_map<AbstractTask*, TransitionContainer>;

public:
    TaskFlow() = default;
    virtual ~TaskFlow() = default;

    // Add a task and return pointer to it
    AbstractTask* addTask(TaskUPtr task);

    // Remove a task
    void removeTask(AbstractTask* task);

    // Add a transition between tasks
    Result addTransition(AbstractTask* from, AbstractTask* to, Result::Code resultCode);
    
    // Change current task
    void changeTaskTo(AbstractTask* newTask);
    
    // Get current task
    AbstractTask* currentTask() const;
    
    // Run current task
    void runCurrentTask();
    
    // Check if there are tasks to run
    bool hasTasksToRun() const;

    // Run the task flow
    void operator()();

private:
    bool isTaskRegistered(AbstractTask* task) const;

private:
    TaskContainer _tasks;
    TransitionMap _transitions;
    AbstractTask* _currTask = nullptr;
};

} // namespace vigine

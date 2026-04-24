#pragma once

/**
 * @file abstracttaskflow.h
 * @brief Legacy task-flow container that sequences AbstractTask instances.
 */

#include "abstracttask.h"
#include "result.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace vigine
{

/**
 * @brief Owns tasks and transitions and drives sequential task execution.
 *
 * Tasks are added by addTask(), linked by addTransition(), and run one
 * at a time via runCurrentTask(). Each task's Result::Code selects the
 * next task from the transition map. A TaskFlow is typically owned by
 * an AbstractState.
 */
class TaskFlow
{
    using TaskUPtr            = std::unique_ptr<AbstractTask>;
    using TaskContainer       = std::vector<TaskUPtr>;
    using Transition          = std::pair<Result::Code, AbstractTask *>;
    using TransitionContainer = std::vector<Transition>;
    using TransitionMap       = std::unordered_map<AbstractTask *, TransitionContainer>;

  public:
    virtual ~TaskFlow() = default;

    // Add a task and return pointer to it
    [[nodiscard]] AbstractTask *addTask(TaskUPtr task);

    // Add a transition between tasks
    [[nodiscard]] Result addTransition(AbstractTask *from, AbstractTask *to,
                                       Result::Code resultCode);

    // Change current task
    void changeTaskTo(AbstractTask *newTask);

    // Get current task
    [[nodiscard]] AbstractTask *currentTask() const;

    // Run current task
    void runCurrentTask();

    // Check if there are tasks to run
    [[nodiscard]] bool hasTasksToRun() const;

  protected:
    TaskFlow() = default;

  private:
    bool isTaskRegistered(AbstractTask *task) const;

  private:
    TaskContainer _tasks;
    TransitionMap _transitions;
    AbstractTask *_currTask = nullptr;
};

} // namespace vigine

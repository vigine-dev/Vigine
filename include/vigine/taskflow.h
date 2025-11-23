#pragma once

#include "abstracttask.h"
#include "result.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace vigine
{
class ISignalBinder;

enum class RouteType
{
    MouseEvent,
    KeyPressEvent,
    KeyRleaseEvent
};

class TaskFlow
{
    using TaskUPtr            = std::unique_ptr<AbstractTask>;
    using TaskContainer       = std::vector<TaskUPtr>;
    using Transition          = std::pair<Result::Code, AbstractTask *>;
    using TransitionContainer = std::vector<Transition>;
    using TransitionMap       = std::unordered_map<AbstractTask *, TransitionContainer>;

  public:
    TaskFlow()          = default;
    virtual ~TaskFlow() = default;

    // Add a task and return pointer to it
    [[nodiscard]] AbstractTask *addTask(TaskUPtr task);

    // Remove a task
    void removeTask(AbstractTask *task);

    // Add a transition between tasks
    [[nodiscard]] Result route(AbstractTask *from, AbstractTask *to,
                               Result::Code resultCode = Result::Code::Success);
    [[nodiscard]] Result signal(AbstractTask *from, AbstractTask *to, ISignalBinder *signal);

    // Change current task
    void changeCurrentTaskTo(AbstractTask *newTask);

    // Get current task
    [[nodiscard]] AbstractTask *currentTask() const;

    // Run current task
    void runCurrentTask();

    // Check if there are tasks to run
    [[nodiscard]] bool hasTasksToRun() const;

    // Run the task flow
    void operator()();

    void setContext(Context *context);

  private:
    bool isTaskRegistered(AbstractTask *task) const;

  private:
    TaskContainer _tasks;
    TransitionMap _transitions;
    AbstractTask *_currTask{nullptr};
    Context *_context{nullptr};
};

} // namespace vigine

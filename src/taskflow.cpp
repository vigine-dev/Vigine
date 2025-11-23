#include <vigine/context.h>
#include <vigine/signal/isignal.h>
#include <vigine/signal/isignalbinder.h>
#include <vigine/signal/isignalemiter.h>
#include <vigine/taskflow.h>

#include <algorithm>

namespace vigine
{

AbstractTask *TaskFlow::addTask(TaskUPtr task)
{
    if (!task)
        return nullptr;

    task->setContext(_context);

    // Store the task
    _tasks.push_back(std::move(task));
    return _tasks.back().get();
}

void TaskFlow::removeTask(AbstractTask *task)
{
    if (!task || !isTaskRegistered(task))
        return;

    // Remove task from current task if it's the one being removed
    if (_currTask == task)
        _currTask = nullptr;

    // Remove all transitions involving this task
    _transitions.erase(task);
    for (auto &[_, transitions] : _transitions)
    {
        transitions.erase(
            std::remove_if(transitions.begin(), transitions.end(),
                           [task](const auto &transition) { return transition.second == task; }),
            transitions.end());
    }

    // Remove the task itself
    _tasks.erase(std::remove_if(_tasks.begin(), _tasks.end(),
                                [task](const auto &t) { return t.get() == task; }),
                 _tasks.end());
}

bool TaskFlow::isTaskRegistered(AbstractTask *task) const
{
    if (!task)
        return false;

    return std::find_if(_tasks.begin(), _tasks.end(),
                        [task](const auto &t) { return t.get() == task; }) != _tasks.end();
}

Result TaskFlow::route(AbstractTask *from, AbstractTask *to, Result::Code resultCode)
{
    if (!from || !to)
        return Result(Result::Code::Error, "Invalid pointer provided for transition");

    if (!isTaskRegistered(from))
        return Result(Result::Code::Error, "From task is not registered");

    if (!isTaskRegistered(to))
        return Result(Result::Code::Error, "To task is not registered");

    _transitions[from].emplace_back(resultCode, to);

    return Result();
}

// TODO: Add a mechanism if we add a new task when we process the signal. Need a new Class that
// checks when we add a new task to process the signal if we are currently doing some signal.The
// same mechanism is needed for the normal task flow.
Result TaskFlow::signal(AbstractTask *from, AbstractTask *to, ISignalBinder *signal)
{
    if (!from || !to || !signal)
        return Result(Result::Code::Error, "Invalid pointer provided for signal transition");

    if (!isTaskRegistered(from))
        return Result(Result::Code::Error, "From task is not registered");

    if (!isTaskRegistered(to))
        return Result(Result::Code::Error, "To task is not registered");

    if (!signal->check(from, to))
        return Result(Result::Code::Error,
                      "Signal check failed: tasks do not satisfy signal interfaces");

    return Result();
}

void TaskFlow::changeCurrentTaskTo(AbstractTask *newTask)
{
    if (!newTask)
        return;

    // Update current task
    _currTask = newTask;
}

AbstractTask *TaskFlow::currentTask() const { return _currTask; }

void TaskFlow::runCurrentTask()
{
    if (!_currTask)
        return;

    // Execute current task and get result
    auto currStatus = _currTask->execute();

    // Check possible transitions
    auto transitions = _transitions.find(_currTask);
    _currTask        = nullptr;

    if (transitions == _transitions.end())
        return;

    for (const auto &[relStatus, relTask] : transitions->second)
    {
        if (relStatus != currStatus.code())
            continue;

        // Found matching transition
        changeCurrentTaskTo(relTask);
        break;
    }
}

bool TaskFlow::hasTasksToRun() const { return _currTask != nullptr; }

void TaskFlow::operator()()
{
    while (hasTasksToRun())
    {
        runCurrentTask();
    }
}

void TaskFlow::setContext(Context *context)
{
    _context = context;

    std::ranges::for_each(_tasks,
                          [&context](const TaskUPtr &taskUptr) { taskUptr->setContext(context); });
}

} // namespace vigine

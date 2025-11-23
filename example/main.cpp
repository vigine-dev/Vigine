#include "vigine/base/function.h"
#include <vigine/signal/isignal.h>
#include <vigine/signal/isignalbinder.h>
#include <vigine/signal/isignalemiter.h>
#include <vigine/statemachine.h>
#include <vigine/taskflow.h>
#include <vigine/vigine.h>

#include "state/closestate.h"
#include "state/errorstate.h"
#include "state/initstate.h"
#include "state/workstate.h"
#include "task/addsomedatatask.h"
#include "task/checkbdshecmetask.h"
#include "task/initbdtask.h"
#include "task/readsomedatatask.h"
#include "task/removesomedatatask.h"

#include <functional>
#include <glm/glm.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <variant>

using namespace vigine;

//  * find best way to add context
//    * Check if the context is adding correctly

// create service Database
// create system for work with DB
// init DB
// make sql query

class KeyRleaseEvent;
class SocketReadEvent;

enum class EventType
{
    MouseClickEvent,
    KeyPressEvent,
    KeyRleaseEvent,
    SocketReadEvent
};

class IEvent : public ISignal
{
  public:
    virtual ~IEvent()                   = default;
    virtual EventType eventType() const = 0;
};

class MouseClickEvent : public IEvent
{
  public:
    MouseClickEvent(int x, int y) : _x(x), _y(y) {}
    ~MouseClickEvent() override = default;
    SignalType type() const override { return SignalType::Event; }
    EventType eventType() const override { return EventType::MouseClickEvent; }
    int x() const { return _x; }
    int y() const { return _y; }

  private:
    int _x;
    int _y;
};

class IMouseEventSignalEmiter : public ISignalEmiter
{
  public:
    virtual ~IMouseEventSignalEmiter() = default;

  protected:
    void emitMouseClickEvent(std::unique_ptr<MouseClickEvent> event)
    {
        if (proxyEmiter())
        {
            proxyEmiter()(event.get());
        }
    }
};

class IMouseEventSignalHandler
{
  public:
    virtual ~IMouseEventSignalHandler() = default;
    virtual void onMouseClickEvent(MouseClickEvent *event) {};
};

class MouseEventSignalBinder : public ISignalBinder
{
  public:
    MouseEventSignalBinder()           = default;
    ~MouseEventSignalBinder() override = default;

    virtual bool check(AbstractTask *taskEmiter, AbstractTask *taskReceiver) override
    {
        auto mouseEventEmiter   = dynamic_cast<IMouseEventSignalEmiter *>(taskEmiter);
        auto mouseEventReceiver = dynamic_cast<IMouseEventSignalHandler *>(taskReceiver);

        if (!mouseEventEmiter || !mouseEventReceiver)
            return false;

        auto procyEmiter = [this](ISignal *signal) {
            if (signal->type() != SignalType::Event)
                return;

            this->emitMouseEvent(dynamic_cast<IEvent *>(signal));
            delete signal;
        };
        mouseEventEmiter->setProxyEmiter(procyEmiter);
        _taskReceiver = mouseEventReceiver;

        return true;
    };

    void emitMouseEvent(IEvent *event)
    {
        switch (event->eventType())
        {
        case EventType::MouseClickEvent: {
            auto mouseEvent = dynamic_cast<MouseClickEvent *>(event);
            if (mouseEvent)
            {
                std::cout << "Mouse clicked at: (" << mouseEvent->x() << ", " << mouseEvent->y()
                          << ")\n";
                if (_taskReceiver)
                {
                    _taskReceiver->onMouseClickEvent(mouseEvent);
                }
            }
            break;
        }
        default:
            break;
        }
    }

  private:
    IMouseEventSignalHandler *_taskReceiver = nullptr;
};

std::unique_ptr<TaskFlow> createInitTaskFlow()
{
    auto taskFlow                  = std::make_unique<TaskFlow>();
    auto mouseEventTaskFlow        = std::make_unique<TaskFlow>();
    auto *initDBTask               = taskFlow->addTask(std::make_unique<InitBDTask>());
    auto *checkBDShecmeTask        = taskFlow->addTask(std::make_unique<CheckBDShecmeTask>());
    auto *runWindow                = taskFlow->addTask(std::make_unique<CheckBDShecmeTask>());
    auto *prcessMouseEventTask     = taskFlow->addTask(std::make_unique<CheckBDShecmeTask>());
    auto *prcessKeyPressEventTask  = taskFlow->addTask(std::make_unique<CheckBDShecmeTask>());
    auto *prcessKeyRleaseEventTask = taskFlow->addTask(std::make_unique<CheckBDShecmeTask>());

    static_cast<void>(mouseEventTaskFlow->route(prcessMouseEventTask,
                                                prcessKeyRleaseEventTask)); // SUCCESS by default

    static_cast<void>(taskFlow->route(initDBTask, checkBDShecmeTask));
    static_cast<void>(taskFlow->route(checkBDShecmeTask, runWindow));

    // taskFlow->signal(runWindow, prcessMouseEventTask, new MouseEventSignalBinder());

    taskFlow->changeCurrentTaskTo(initDBTask);

    return taskFlow;
}

std::unique_ptr<TaskFlow> createWorkTaskFlow()
{
    auto taskFlow            = std::make_unique<TaskFlow>();
    auto *addSomeDataTask    = taskFlow->addTask(std::make_unique<AddSomeDataTask>());
    auto *readSomeDataTask   = taskFlow->addTask(std::make_unique<ReadSomeDataTask>());
    auto *removeSomeDataTask = taskFlow->addTask(std::make_unique<RemoveSomeDataTask>());

    // taskFlow->addTransition(addSomeDataTask, readSomeDataTask, Result::Code::Success);
    // taskFlow->addTransition(readSomeDataTask, removeSomeDataTask, Result::Code::Success);
    // taskFlow->changeCurrentTaskTo(addSomeDataTask);

    return taskFlow;
}

std::unique_ptr<TaskFlow> createErrorTaskFlow()
{
    auto taskFlow = std::make_unique<TaskFlow>();

    return taskFlow;
}

std::unique_ptr<TaskFlow> createCloseTaskFlow()
{
    auto taskFlow = std::make_unique<TaskFlow>();

    return taskFlow;
}

int main()
{
    Engine engine;
    StateMachine *stMachine = engine.state();

    // init states
    auto initState  = std::make_unique<InitState>();
    auto workState  = std::make_unique<WorkState>();
    auto errorState = std::make_unique<ErrorState>();
    auto closeState = std::make_unique<CloseState>();

    // Add states to Vigine
    auto initPtr       = stMachine->addState(std::move(initState));
    auto workPtr       = stMachine->addState(std::move(workState));
    auto errorPtr      = stMachine->addState(std::move(errorState));
    auto closePtr      = stMachine->addState(std::move(closeState));

    auto initTaskFlow  = createInitTaskFlow();
    auto workTaskFlow  = createWorkTaskFlow();
    auto errorTaskFlow = createErrorTaskFlow();
    auto closeTaskFlow = createCloseTaskFlow();

    initPtr->setTaskFlow(std::move(initTaskFlow));
    workPtr->setTaskFlow(std::move(workTaskFlow));
    errorPtr->setTaskFlow(std::move(errorTaskFlow));
    closePtr->setTaskFlow(std::move(closeTaskFlow));

    // Add transitions
    static_cast<void>(stMachine->addTransition(initPtr, workPtr, Result::Code::Success));
    static_cast<void>(stMachine->addTransition(initPtr, errorPtr, Result::Code::Error));

    static_cast<void>(stMachine->addTransition(workPtr, closePtr, Result::Code::Success));
    static_cast<void>(stMachine->addTransition(workPtr, errorPtr, Result::Code::Error));

    static_cast<void>(stMachine->addTransition(errorPtr, workPtr, Result::Code::Success));
    static_cast<void>(stMachine->addTransition(errorPtr, closePtr, Result::Code::Error));

    stMachine->changeStateTo(initPtr);

    engine.run();

    return 0;
}

#include "concepts.h"

#include "vigine/result.h"
#include "vigine/vigine.h"
#include <vigine/abstractstate.h>
#include <vigine/abstracttask.h>
#include <vigine/component/componentmanager.h>
#include <vigine/ecs/entity.h>
#include <vigine/statemachine.h>
#include <vigine/taskflow.h>
#include <vigine/vigine.h>

#include <concepts>
#include <functional>
#include <gtest/gtest.h>
#include <memory>

using namespace vigine;

class OpenState : public AbstractState
{
  public:
    ~OpenState() override {}

    void enter() override
    {
        // Open state entry logic
        std::cout << "OpenState enter" << std::endl;
    }

    Result exit() override
    {
        // Open state exit logic
        return Result();
    }
};

class WorkState : public AbstractState
{
  public:
    void enter() override
    {
        // Work state entry logic
        std::cout << "WorkState enter" << std::endl;
    }

    Result exit() override
    {
        // Work state exit logic
        return Result();
    }
};

class ErrorState : public AbstractState
{
  public:
    void enter() override
    {
        // Error state entry logic
    }

    Result exit() override
    {
        // Error state exit logic
        return Result();
    }
};

class CloseState : public AbstractState
{
  public:
    void enter() override
    {
        // Close state entry logic
        std::cout << "WorkState enter" << std::endl;
    }

    Result exit() override
    {
        // Close state exit logic
        return Result();
    }
};

// Test task for TaskFlow
class TestTask : public AbstractTask
{
  public:
    TestTask(int &val, int inc) : _val{val} { _val += inc; }

  protected:
    Result execute() override { return Result(); }

  private:
    int &_val;
};

TEST(ArchitectureTest, Vigine_run_void)
{
    auto vigine            = std::make_unique<vigine::Engine>();
    StateMachine *stateMch = vigine->state();

    int result             = 0;

    // Create states with TaskFlows
    auto state1 = std::make_unique<OpenState>();
    auto state2 = std::make_unique<WorkState>();
    auto state3 = std::make_unique<CloseState>();

    // Create TaskFlows with tasks for each state
    auto taskFlow1 = std::make_unique<TaskFlow>();
    auto taskFlow2 = std::make_unique<TaskFlow>();
    auto taskFlow3 = std::make_unique<TaskFlow>();

    // Add tasks to TaskFlows
    auto task1 = std::make_unique<TestTask>(result, 1);
    auto task2 = std::make_unique<TestTask>(result, 1);
    auto task3 = std::make_unique<TestTask>(result, 5);

    taskFlow1->addTask(std::move(task1));
    taskFlow2->addTask(std::move(task2));
    taskFlow3->addTask(std::move(task3));

    // Set TaskFlows to states
    state1->setTaskFlow(std::move(taskFlow1));
    state2->setTaskFlow(std::move(taskFlow2));
    state3->setTaskFlow(std::move(taskFlow3));

    // Add states to Vigine
    auto state1Ptr = state1.get();
    auto state2Ptr = state2.get();
    auto state3Ptr = state3.get();

    stateMch->addState(std::move(state1));
    stateMch->addState(std::move(state2));
    stateMch->addState(std::move(state3));

    // Add transitions
    stateMch->addTransition(state1Ptr, state2Ptr, Result::Code::Success);
    stateMch->addTransition(state2Ptr, state3Ptr, Result::Code::Success);

    // Set initial state
    stateMch->changeStateTo(state1Ptr);

    // Run Vigine
    ASSERT_NO_THROW(vigine->run()) << "run() should not throw";
    EXPECT_EQ(result, 7);
}

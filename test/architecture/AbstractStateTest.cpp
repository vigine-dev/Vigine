#include <gtest/gtest.h>

#include <vigine/abstractstate.h>
#include <vigine/taskflow.h>
#include <vigine/abstracttask.h>
#include "concepts.h"
#include "vigine/result.h"

#include <memory>
#include <concepts>

using namespace vigine;

// Test class for AbstractState
class TestState : public AbstractState {
protected:
    void enter() override {}
    Result exit() override { return Result(); }
};

// Test class that exposes protected methods for testing
class TestStateExposed : public AbstractState {
public:
    void enter() override {}
    Result exit() override { return Result(); }

    void setTaskFlow(std::unique_ptr<TaskFlow> taskFlow) {
        AbstractState::setTaskFlow(std::move(taskFlow));
    }

    TaskFlow* getTaskFlow() const {
        return AbstractState::getTaskFlow();
    }
};

TEST(AbstractStateTest, check_isAbstract)
{
    EXPECT_TRUE((IsAbstract<AbstractState>))
        << "AbstractState isn't an abstract";
}

TEST(AbstractStateTest, method_destructor)
{
    EXPECT_TRUE((HasMethod_destructor<AbstractState>))
        << "AbstractState hasn't correct destructor";
}

TEST(AbstractStateTest, constructor_empty)
{
    std::unique_ptr<AbstractState> state;
    ASSERT_NO_THROW(state = std::make_unique<TestState>());
    ASSERT_NE(state, nullptr);
}

TEST(AbstractStateTest, operator_execution)
{
    auto state = std::make_unique<TestStateExposed>();
    auto taskFlow = std::make_unique<TaskFlow>();
    state->setTaskFlow(std::move(taskFlow));
    
    // Test operator execution
    ASSERT_NO_THROW((*state)()) << "operator() should not throw";
}

TEST(AbstractStateTest, enter_empty_void)
{
    auto state = std::make_unique<TestStateExposed>();
    ASSERT_NO_THROW(state->enter()) << "enter() should not throw";
}

TEST(AbstractStateTest, exit_empty_Result)
{
    auto state = std::make_unique<TestStateExposed>();
    auto result = state->exit();
    ASSERT_EQ(result.code(), Result::Code::Success) << "exit() should return Success";
}

TEST(AbstractStateTest, setTaskFlow_empty_void)
{
    auto state = std::make_unique<TestStateExposed>();
    auto taskFlow = std::make_unique<TaskFlow>();
    ASSERT_NO_THROW(state->setTaskFlow(std::move(taskFlow))) << "setTaskFlow() should not throw";
}

TEST(AbstractStateTest, getTaskFlow_empty_TaskFlow_pointer)
{
    auto state = std::make_unique<TestStateExposed>();
    EXPECT_EQ(nullptr, state->getTaskFlow()) << "getTaskFlow() should return nullptr when no task flow is set";
    
    auto taskFlow = std::make_unique<TaskFlow>();
    TaskFlow* taskFlowPtr = taskFlow.get();
    state->setTaskFlow(std::move(taskFlow));
    EXPECT_EQ(taskFlowPtr, state->getTaskFlow()) << "getTaskFlow() should return the correct task flow";
}


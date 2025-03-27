#include <gtest/gtest.h>

#include <vigine/abstractstate.h>
#include <vigine/abstracttaskflow.h>
#include "concepts.h"
#include "vigine/result.h"

#include <memory>
#include <concepts>

using namespace vigine;

// Test class for AbstractTaskFlow
class TestTaskFlow : public AbstractTaskFlow {
public:
    void operator()() override {
        // Test implementation
    }

    void addTask(AbstractTask* task, const RelationContainer& relations) override {
        // Test implementation
    }

    void removeTask(AbstractTask* task) override {
        // Test implementation
    }

    void update() override {
        // Test implementation
    }
};

// Concepts for checking AbstractState methods
template <typename T>
concept MethodCheck_Enter = requires(T t)
{
    t.enter();
    { t.enter() } -> std::same_as<void>;
};

template <typename T>
concept MethodCheck_Exit = requires(T t)
{
    t.exit();
    { t.exit() } -> std::same_as<Result>;
};

template <typename T>
concept MethodCheck_Update = requires(T t)
{
    t.update();
    { t.update() } -> std::same_as<void>;
};

template <typename T>
concept MethodCheck_SetTaskFlow = requires(T t)
{
    t.setTaskFlow(std::make_unique<TestTaskFlow>());
    { t.setTaskFlow(std::make_unique<TestTaskFlow>()) } -> std::same_as<void>;
};

template <typename T>
concept MethodCheck_GetTaskFlow = requires(T t)
{
    t.getTaskFlow();
    { t.getTaskFlow() } -> std::same_as<AbstractTaskFlow*>;
};

// Test class for AbstractState
class TestState : public AbstractState {
public:
    void enter() override {}
    Result exit() override { return Result(); }
    void update() override {}
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

TEST(AbstractStateTest, method_enter)
{
    EXPECT_TRUE((HasMethod_enter<AbstractState, void>))
        << "AbstractState hasn't correct enter method";
}

TEST(AbstractStateTest, method_exit)
{
    EXPECT_TRUE((HasMethod_exit<AbstractState, Result>))
        << "AbstractState hasn't correct exit method";
}

TEST(AbstractStateTest, method_update)
{
    EXPECT_TRUE((HasMethod_update<AbstractState, void>))
        << "AbstractState hasn't correct update method";
}

TEST(AbstractStateTest, enter_empty_void)
{
    EXPECT_TRUE(MethodCheck_Enter<AbstractState>) << "AbstractState hasn't expected void enter() method";
}

TEST(AbstractStateTest, exit_empty_void)
{
    EXPECT_TRUE(MethodCheck_Exit<AbstractState>) << "AbstractState hasn't expected Result exit() method";
}

TEST(AbstractStateTest, update_empty_void)
{
    EXPECT_TRUE(MethodCheck_Update<AbstractState>) << "AbstractState hasn't expected void update() method";
}

TEST(AbstractStateTest, setTaskFlow_empty_void)
{
    EXPECT_TRUE(MethodCheck_SetTaskFlow<AbstractState>) << "AbstractState hasn't expected void setTaskFlow() method";
}

TEST(AbstractStateTest, getTaskFlow_empty_AbstractTaskFlow_pointer)
{
    EXPECT_TRUE(MethodCheck_GetTaskFlow<AbstractState>) << "AbstractState hasn't expected AbstractTaskFlow* getTaskFlow() method";
}

TEST(AbstractStateTest, task_flow_management)
{
    auto state = std::make_unique<TestState>();
    
    // Test setting a task flow
    auto taskFlow = std::make_unique<TestTaskFlow>();
    AbstractTaskFlow* taskFlowPtr = taskFlow.get();
    ASSERT_NO_THROW(state->setTaskFlow(std::move(taskFlow)));
    
    // Test getting the task flow
    EXPECT_EQ(taskFlowPtr, state->getTaskFlow()) << "getTaskFlow() should return the correct task flow";
    
    // Test updating with task flow
    ASSERT_NO_THROW(state->update()) << "update() should not throw when task flow is set";
    
    // Test updating without task flow
    state->setTaskFlow(nullptr);
    ASSERT_NO_THROW(state->update()) << "update() should not throw when no task flow is set";
}


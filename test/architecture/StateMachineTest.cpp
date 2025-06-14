#include <gtest/gtest.h>

#include <vigine/vigine.h>
#include <vigine/statemachine.h>
#include <vigine/abstractstate.h>
#include <vigine/taskflow.h>
#include "concepts.h"
#include "vigine/result.h"

#include <memory>
#include <concepts>

using namespace vigine;

class SomeState: public AbstractState
{
protected:
    void enter() override 
    {
        // Test state enter logic
    }
    
    Result exit() override 
    {
        // Test state exit logic
        return Result();
    }
};

class SomeState2: public AbstractState
{
protected:
    void enter() override 
    {
        // Test state2 enter logic
    }
    
    Result exit() override 
    {
        // Test state2 exit logic
        return Result();
    }
};

template <typename T>
concept MethodCheck_changeStateTo = requires(T t)
{
    t.changeStateTo(std::make_unique<SomeState>());
    { t.changeStateTo(std::make_unique<SomeState>()) } -> std::same_as<void>;
};

template <typename T>
concept MethodCheck_currentState = requires(T t)
{
    t.currentState();
    { t.currentState() } -> std::same_as<AbstractState*>;
};

template <typename T>
concept MethodCheck_runCurrentState = requires(T t)
{
    t.runCurrentState();
    { t.runCurrentState() } -> std::same_as<void>;
};

template <typename T>
concept MethodCheck_hasStatesToRun = requires(T t)
{
    t.hasStatesToRun();
    { t.hasStatesToRun() } -> std::same_as<bool>;
};

// Test class for TaskFlow
class TestTaskFlow {
public:
    void operator()() {
        // Test implementation
    }

    AbstractTask* addTask(std::unique_ptr<AbstractTask> task) {
        // Test implementation
        return task.get();
    }

    void removeTask(AbstractTask* task) {
        // Test implementation
    }

    void update() {
        // Test implementation
    }

    void addTransition(AbstractTask* from, AbstractTask* to, Result::Code code) {
        // Test implementation
    }

    void changeTaskTo(AbstractTask* task) {
        // Test implementation
    }

    AbstractTask* currentTask() const {
        // Test implementation
        return nullptr;
    }

    bool hasTasksToRun() const {
        // Test implementation
        return false;
    }

    void runCurrentTask() {
        // Test implementation
    }
};

// Test class for AbstractState
class TestState : public AbstractState {
protected:
    void enter() override {}
    Result exit() override { return Result(); }
};

// Test class that exposes protected methods for testing
class TestStateExposed : public AbstractState {
public:
    using AbstractState::enter;
    using AbstractState::exit;
    using AbstractState::setTaskFlow;
    using AbstractState::getTaskFlow;
};

TEST(StateMachineTest, method_destructor)
{
    EXPECT_TRUE((HasMethod_destructor<StateMachine>))
        << "StateMachine hasn't correct destructor";
}

TEST(StateMachineTest, constructor_empty)
{
    std::unique_ptr<StateMachine> stateMachine;
    ASSERT_NO_THROW(stateMachine = std::make_unique<StateMachine>());
    ASSERT_NE(stateMachine, nullptr);
}

TEST(StateMachineTest, addState)
{
    auto stateMachine = std::make_unique<StateMachine>();
    auto someState = std::make_unique<SomeState>();
    SomeState* rawPtrSomeState = someState.get();
    
    auto returnedPtr = stateMachine->addState(std::move(someState));
    ASSERT_EQ(rawPtrSomeState, returnedPtr);
    ASSERT_EQ(nullptr, stateMachine->currentState());
}

TEST(StateMachineTest, addTransition)
{
    auto stateMachine = std::make_unique<StateMachine>();
    
    // Add states
    auto state1 = std::make_unique<SomeState>();
    auto state2 = std::make_unique<SomeState2>();
    auto state1Ptr = state1.get();
    auto state2Ptr = state2.get();
    
    auto returnedState1Ptr = stateMachine->addState(std::move(state1));
    auto returnedState2Ptr = stateMachine->addState(std::move(state2));
    
    ASSERT_EQ(state1Ptr, returnedState1Ptr);
    ASSERT_EQ(state2Ptr, returnedState2Ptr);
    
    // Add transition
    auto result = stateMachine->addTransition(state1Ptr, state2Ptr, Result::Code::Error);
    ASSERT_TRUE(result.isSuccess()) << "addTransition should succeed";
    
    // Change state and run
    stateMachine->changeStateTo(state1Ptr);
    stateMachine->runCurrentState();
    
    // Check if transition occurred
    ASSERT_EQ(stateMachine->currentState(), state2Ptr);
}

TEST(StateMachineTest, hasStatesToRun)
{
    auto stateMachine = std::make_unique<StateMachine>();
    
    // Check without state
    ASSERT_FALSE(stateMachine->hasStatesToRun());
    
    // Add state
    auto state = std::make_unique<SomeState>();
    auto statePtr = state.get();
    auto returnedStatePtr = stateMachine->addState(std::move(state));
    stateMachine->changeStateTo(statePtr);

    ASSERT_EQ(statePtr, returnedStatePtr);
    ASSERT_TRUE(stateMachine->hasStatesToRun());
}

TEST(StateMachineTest, runCurrentState_without_transition)
{
    auto stateMachine = std::make_unique<StateMachine>();
    
    // Add state
    auto state = std::make_unique<SomeState>();
    auto statePtr = state.get();
    stateMachine->addState(std::move(state));
    stateMachine->changeStateTo(statePtr);
    
    // Run state without transition
    stateMachine->runCurrentState();
    
    // State should remain the same after run without transition
    ASSERT_EQ(stateMachine->currentState(), nullptr);
}

TEST(StateMachineTest, changeStateTo)
{
    auto stateMachine = std::make_unique<StateMachine>();
    
    // Add states
    auto state1 = std::make_unique<SomeState>();
    auto state2 = std::make_unique<SomeState2>();
    auto state1Ptr = state1.get();
    auto state2Ptr = state2.get();
    
    stateMachine->addState(std::move(state1));
    stateMachine->addState(std::move(state2));
    
    // Change to first state
    stateMachine->changeStateTo(state1Ptr);
    ASSERT_EQ(stateMachine->currentState(), state1Ptr);
    
    // Change to second state
    stateMachine->changeStateTo(state2Ptr);
    ASSERT_EQ(stateMachine->currentState(), state2Ptr);
}

TEST(StateMachineTest, currentState)
{
    auto stateMachine = std::make_unique<StateMachine>();
    
    // Initially should be nullptr
    ASSERT_EQ(nullptr, stateMachine->currentState());
    
    // Add and change state
    auto state = std::make_unique<SomeState>();
    auto statePtr = state.get();
    stateMachine->addState(std::move(state));
    stateMachine->changeStateTo(statePtr);
    
    // Should return current state
    ASSERT_EQ(statePtr, stateMachine->currentState());
}

#include <gtest/gtest.h>

#include <vigine/vigine.h>
#include <vigine/statemachine.h>

#include <memory>

using namespace vigine;

class SomeState: public AbstractState
{
public:
    ~SomeState() override{};
    
    void enter() override 
    {
        // Test state enter logic
    }
    
    Result exit() override 
    {
        // Test state exit logic
        return Result();
    }
    
    void update() override 
    {
        // Test state update logic
    }
};

class SomeState2: public AbstractState
{
public:
    ~SomeState2() override{};
    
    void enter() override 
    {
        // Test state2 enter logic
    }
    
    Result exit() override 
    {
        // Test state2 exit logic
        return Result();
    }
    
    void update() override 
    {
        // Test state2 update logic
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

TEST(StateMachineTest, constructor_empty)
{
    std::unique_ptr<StateMachine> stateMachine;
    ASSERT_NO_THROW(stateMachine = std::make_unique<StateMachine>());
    ASSERT_NE(stateMachine, nullptr);
    ASSERT_EQ(stateMachine->currentState(), nullptr) << "Initial state should be nullptr";
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
    
    // Add status and transition
    stateMachine->addTransition(state1Ptr, state2Ptr, Result::Code::Success);
    stateMachine->changeStateTo(state1Ptr);

    // Run state
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
    stateMachine->addState(std::move(state));
    
    // Run state without transition
    stateMachine->runCurrentState();
    
    // State should be nullptr after exit
    ASSERT_EQ(stateMachine->currentState(), nullptr);
}

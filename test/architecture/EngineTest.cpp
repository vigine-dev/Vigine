#include <gtest/gtest.h>

#include <vigine/vigine.h>
#include <vigine/statemachine.h>
#include <vigine/abstractstate.h>
#include <vigine/abstractstatus.h>

#include <memory>

using namespace vigine;

class TestState : public AbstractState {
public:
    void enter() override {}
    Result exit() override { return Result(); }
    void update() override {}
};

TEST(EngineTest, constructor_empty)
{
    std::unique_ptr<Engine> engine;
    ASSERT_NO_THROW(engine = std::make_unique<Engine>());
    ASSERT_NE(engine, nullptr);
}

TEST(EngineTest, state)
{
    auto engine = std::make_unique<Engine>();
    ASSERT_NE(engine->state(), nullptr);
}

TEST(EngineTest, run_without_states)
{
    auto engine = std::make_unique<Engine>();
    ASSERT_NO_THROW(engine->run());
}

TEST(EngineTest, state_management)
{
    auto engine = std::make_unique<Engine>();
    auto stateMachine = engine->state();
    ASSERT_NE(stateMachine, nullptr);
    
    // Test state machine functionality through engine
    auto state = std::make_unique<TestState>();
    auto statePtr = state.get();
    stateMachine->addState(std::move(state));
    
    ASSERT_EQ(stateMachine->currentState(), statePtr);
    ASSERT_TRUE(stateMachine->hasStatesToRun());
}

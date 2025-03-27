#include <gtest/gtest.h>

#include <vigine/abstractstate.h>
#include <vigine/statemachine.h>
#include <vigine/component/componentmanager.h>
#include <vigine/entity.h>
#include <vigine/vigine.h>

#include <functional>
#include <memory>

using namespace vigine;

class OpenState : public AbstractState {
public:
  OpenState(int &val) : _val{val} {}
  ~OpenState() override {}

  void enter() override {
    // Open state entry logic
      ++_val;
    std::cout << "OpenState enter" << std::endl;
  }
    
    Result exit() override {
        // Open state exit logic
        return Result();
    }
    
    void update() override {
        // Open state update logic
    }

private:
    int &_val;
};

class WorkState : public AbstractState {
public:
    WorkState(int &val) : _val{val} {}
    void enter() override {
        // Work state entry logic
        _val += 1;
        std::cout << "WorkState enter" << std::endl;
    }
    
    Result exit() override {
        // Work state exit logic
        return Result();
    }
    
    void update() override {
        // Work state update logic
    }

private:
    int &_val;
};

class ErrorState : public AbstractState {
public:
    void enter() override {
        // Error state entry logic
    }
    
    Result exit() override {
        // Error state exit logic
        return Result();
    }
    
    void update() override {
        // Error state update logic
    }
};

class CloseState : public AbstractState {
public:
    CloseState(int &val) : _val{val} {}
    void enter() override {
        // Close state entry logic
        _val += 5;
        std::cout << "WorkState enter" << std::endl;
    }
    
    Result exit() override {
        // Close state exit logic
        return Result();
    }
    
    void update() override {
        // Close state update logic
    }

private:
    int &_val;
};

TEST(ArchitectureTest, Vigine_run_void)
{
  vigine::Engine vigine;
  StateMachine *stateMch = vigine.state();

  int checkResult = 0;
  auto *openState = stateMch->addState(std::make_unique<OpenState>(checkResult));
  auto *workState = stateMch->addState(std::make_unique<WorkState>(checkResult));
  auto *closeState = stateMch->addState(std::make_unique<CloseState>(checkResult));

  stateMch->addTransition(openState, workState, Result::Code::Success);
  stateMch->addTransition(workState, closeState, Result::Code::Success);
  stateMch->changeStateTo(openState);

  vigine.run();

  ASSERT_EQ(checkResult, 7);
}

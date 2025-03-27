#pragma once

#include <memory>

namespace vigine {

class StateMachine;

class Engine {
public:
    Engine();
    void run();
    StateMachine* state();

private:
    std::unique_ptr<StateMachine> _stateMachine;
};

} // namespace vigine

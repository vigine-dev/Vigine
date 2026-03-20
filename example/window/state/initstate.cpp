#include "initstate.h"

#include <print>

InitState::InitState() {}

void InitState::enter() { std::println("run InitState::enter()"); }

vigine::Result InitState::exit() { return vigine::Result(); }

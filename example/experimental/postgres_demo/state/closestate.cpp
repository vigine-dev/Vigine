#include "closestate.h"

#include <print>

CloseState::CloseState() {}

void CloseState::enter() { std::println("run CloseState::enter()"); }

vigine::Result CloseState::exit() { return vigine::Result(); }

#include "workstate.h"

#include <print>

WorkState::WorkState() {}

void WorkState::enter() { std::println("run WorkState::enter()"); }

vigine::Result WorkState::exit() { return vigine::Result(vigine::Result::Code::Error, "Some error"); }

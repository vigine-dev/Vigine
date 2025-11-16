#include "errorstate.h"

#include <print>

ErrorState::ErrorState() {}

void ErrorState::enter() { std::println("run ErrorState::enter()"); }

vigine::Result ErrorState::exit() { return vigine::Result(vigine::Result::Code::Error, "Need to close programm"); }

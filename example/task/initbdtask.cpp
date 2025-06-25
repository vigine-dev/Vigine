#include "initbdtask.h"

#include <print>

InitBDTask::InitBDTask() {}

vigine::Result InitBDTask::execute() {
  std::println("-- InitBDTask::execute()");

  return vigine::Result();
}

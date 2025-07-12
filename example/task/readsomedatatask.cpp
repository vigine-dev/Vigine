#include "readsomedatatask.h"

#include <print>

ReadSomeDataTask::ReadSomeDataTask() {}

vigine::Result ReadSomeDataTask::execute() {
  std::println("-- ReadSomeDataTask::execute()");

  return vigine::Result();
}

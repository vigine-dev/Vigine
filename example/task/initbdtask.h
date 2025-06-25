#pragma once

#include <vigine/abstracttask.h>

class InitBDTask : public vigine::AbstractTask {
public:
  InitBDTask();

  vigine::Result execute() override;
};

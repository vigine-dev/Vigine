#pragma once

#include <vigine/abstracttask.h>

class CheckBDShecmeTask : public vigine::AbstractTask
{
  public:
    CheckBDShecmeTask();

    vigine::Result execute() override;
};

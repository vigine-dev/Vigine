#pragma once

#include <vigine/abstracttask.h>

class SetupCubeTask : public vigine::AbstractTask
{
  public:
    SetupCubeTask();

    void contextChanged() override;
    vigine::Result execute() override;

  private:
};

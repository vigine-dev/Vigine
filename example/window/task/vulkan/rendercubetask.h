#pragma once

#include <vigine/api/taskflow/abstracttask.h>

class RenderCubeTask : public vigine::AbstractTask
{
  public:
    RenderCubeTask();

    void contextChanged() override;
    vigine::Result execute() override;

  private:
    float _rotationAngle{0.0f};
};

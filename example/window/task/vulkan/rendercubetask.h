#pragma once

#include <vigine/api/taskflow/abstracttask.h>

class RenderCubeTask : public vigine::AbstractTask
{
  public:
    RenderCubeTask();

    void contextChanged() override;
    [[nodiscard]] vigine::Result run() override;

  private:
    float _rotationAngle{0.0f};
};

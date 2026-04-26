#pragma once

#include <vigine/api/taskflow/abstracttask.h>

class SetupCubeTask final : public vigine::AbstractTask
{
  public:
    SetupCubeTask();

    [[nodiscard]] vigine::Result run() override;
};

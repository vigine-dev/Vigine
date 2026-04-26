#pragma once

#include <vigine/api/taskflow/abstracttask.h>

class SetupTextTask final : public vigine::AbstractTask
{
  public:
    SetupTextTask();

    [[nodiscard]] vigine::Result run() override;
};

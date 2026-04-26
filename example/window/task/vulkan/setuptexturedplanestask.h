#pragma once

#include <vigine/api/taskflow/abstracttask.h>

class SetupTexturedPlanesTask final : public vigine::AbstractTask
{
  public:
    SetupTexturedPlanesTask() = default;

    [[nodiscard]] vigine::Result run() override;
};

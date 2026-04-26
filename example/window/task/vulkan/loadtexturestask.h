#pragma once

#include <vigine/api/taskflow/abstracttask.h>

class LoadTexturesTask final : public vigine::AbstractTask
{
  public:
    LoadTexturesTask() = default;

    [[nodiscard]] vigine::Result run() override;
};

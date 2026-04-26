#pragma once

#include <vigine/api/taskflow/abstracttask.h>

class InitVulkanTask final : public vigine::AbstractTask
{
  public:
    InitVulkanTask();

    [[nodiscard]] vigine::Result run() override;
};

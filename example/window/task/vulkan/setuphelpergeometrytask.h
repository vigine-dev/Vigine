#pragma once

#include <vigine/api/taskflow/abstracttask.h>

class SetupHelperGeometryTask final : public vigine::AbstractTask
{
  public:
    SetupHelperGeometryTask();

    [[nodiscard]] vigine::Result run() override;
};

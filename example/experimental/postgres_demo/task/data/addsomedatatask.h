#pragma once

#include <vigine/api/taskflow/abstracttask.h>

namespace vigine
{
class DatabaseService;
}

class AddSomeDataTask : public vigine::AbstractTask
{
  public:
    AddSomeDataTask();

    void contextChanged() override;
    [[nodiscard]] vigine::Result run() override;

  private:
    vigine::DatabaseService *_dbService{nullptr};
};

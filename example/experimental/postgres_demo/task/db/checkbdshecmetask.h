#pragma once

#include <vigine/api/taskflow/abstracttask.h>

namespace vigine
{
class DatabaseService;
}

class CheckBDShecmeTask : public vigine::AbstractTask
{
  public:
    CheckBDShecmeTask();

    void setDatabaseService(vigine::DatabaseService *service);

    [[nodiscard]] vigine::Result run() override;

  private:
    vigine::DatabaseService *_dbService{nullptr};
};

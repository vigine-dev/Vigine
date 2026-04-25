#pragma once

#include <vigine/abstracttask.h>

namespace vigine
{
class DatabaseService;
}

class CheckBDShecmeTask : public vigine::AbstractTask
{
  public:
    CheckBDShecmeTask();

    void contextChanged() override;
    vigine::Result execute() override;

  private:
    vigine::DatabaseService *_dbService{nullptr};
};

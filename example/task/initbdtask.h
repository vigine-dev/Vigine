#pragma once

#include <vigine/abstracttask.h>

namespace vigine
{
class DatabaseService;
}

class InitBDTask : public vigine::AbstractTask
{
  public:
    InitBDTask();

    void contextChanged() override;
    vigine::Result execute() override;

  private:
    vigine::DatabaseService *_dbService{nullptr};
};

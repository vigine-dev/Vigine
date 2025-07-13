#pragma once

#include <vigine/abstracttask.h>

namespace vigine
{
class DatabaseService;
}

class ReadSomeDataTask : public vigine::AbstractTask
{
  public:
    ReadSomeDataTask();

    void contextChanged() override;
    vigine::Result execute() override;

  private:
    vigine::DatabaseService *_dbService{nullptr};
};

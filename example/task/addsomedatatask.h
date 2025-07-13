#pragma once

#include <vigine/abstracttask.h>

namespace vigine
{
class DatabaseService;
}

class AddSomeDataTask : public vigine::AbstractTask
{
  public:
    AddSomeDataTask();

    void contextChanged() override;
    vigine::Result execute() override;

  private:
    vigine::DatabaseService *_dbService{nullptr};
};

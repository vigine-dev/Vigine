#pragma once

#include "vigine/abstracttask.h"

namespace vigine
{
class DatabaseService;
}

class RemoveSomeDataTask : public vigine::AbstractTask
{
  public:
    RemoveSomeDataTask();

    void contextChanged() override;
    vigine::Result execute() override;

  private:
    vigine::DatabaseService *_dbService{nullptr};
};

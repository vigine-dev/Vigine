#pragma once

#include <vigine/abstracttask.h>

class AddSomeDataTask : public vigine::AbstractTask
{
  public:
    AddSomeDataTask();
    vigine::Result execute() override;
};

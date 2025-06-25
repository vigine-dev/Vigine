#pragma once

#include <vigine/abstracttask.h>

class ReadSomeDataTask : public vigine::AbstractTask {
public:
  ReadSomeDataTask();
  vigine::Result execute() override;
};

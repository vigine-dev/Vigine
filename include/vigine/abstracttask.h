#pragma once

#include "result.h"

namespace vigine {

class AbstractTask {

public:
  virtual ~AbstractTask() = default;
    virtual Result execute() = 0;

protected:
  AbstractTask() = default;
};

} // namespace vigine

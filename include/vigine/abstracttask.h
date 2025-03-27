#pragma once

namespace vigine {

class AbstractTask {

public:
  virtual ~AbstractTask() = default;
    virtual void execute() = 0;

protected:
  AbstractTask() = default;
};

} // namespace vigine

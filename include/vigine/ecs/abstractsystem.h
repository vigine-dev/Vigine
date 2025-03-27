#pragma once

namespace vigine {

class AbstractSystem {
public:
  virtual ~AbstractSystem() = 0;

protected:
  AbstractSystem() = default;
};

} // namespace vigine

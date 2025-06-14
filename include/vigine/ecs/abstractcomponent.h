#pragma once

namespace vigine {

class AbstractComponent {
public:
  virtual ~AbstractComponent() = 0;

protected:
  AbstractComponent() = default;
};

} // namespace vigine

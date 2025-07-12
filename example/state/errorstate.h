#pragma once

#include <vigine/abstractstate.h>

class ErrorState : public vigine::AbstractState {
public:
  ErrorState();

protected:
  virtual void enter();
  virtual vigine::Result exit();
};

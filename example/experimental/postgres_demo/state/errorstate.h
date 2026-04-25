#pragma once

#include <vigine/api/statemachine/abstractstate.h>

class ErrorState : public vigine::AbstractState
{
  public:
    ErrorState();

  protected:
    virtual void enter();
    virtual vigine::Result exit();
};

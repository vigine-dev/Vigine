#pragma once

#include <vigine/api/statemachine/abstractstate.h>

class CloseState : public vigine::AbstractState
{
  public:
    CloseState();

  protected:
    virtual void enter();
    virtual vigine::Result exit();
};

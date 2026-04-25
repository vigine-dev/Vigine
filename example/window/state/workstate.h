#pragma once

#include <vigine/api/statemachine/abstractstate.h>

class WorkState : public vigine::AbstractState
{
  public:
    WorkState();

  protected:
    virtual void enter();
    virtual vigine::Result exit();
};

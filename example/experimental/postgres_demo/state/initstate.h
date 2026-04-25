#pragma once

#include <vigine/api/statemachine/abstractstate.h>

class InitState final : public vigine::AbstractState
{
  public:
    InitState();

  protected:
    virtual void enter();
    virtual vigine::Result exit();
};

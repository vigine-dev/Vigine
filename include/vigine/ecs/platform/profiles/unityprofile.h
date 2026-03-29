#pragma once
#include "vigine/ecs/platform/inputprofilecomponent.h"

namespace vigine { namespace platform {

class UnityProfile : public InputProfileComponent
{
  public:
    UnityProfile();
    const char* name() const override { return "Unity"; }
    const char* description() const override { return "Unity-style: W/E/R for Move/Rotate/Scale, Ctrl+D duplicate, F frame"; }
  protected:
    void populateBindings() override;
};

}} // namespace vigine::platform

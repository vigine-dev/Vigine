#pragma once
#include "vigine/ecs/platform/inputprofilecomponent.h"

namespace vigine { namespace platform {

class Cinema4DProfile : public InputProfileComponent
{
  public:
    Cinema4DProfile();
    const char* name() const override { return "Cinema 4D"; }
    const char* description() const override { return "Cinema 4D-style: E/R/T for Move/Rotate/Scale, S for frame selected"; }
  protected:
    void populateBindings() override;
};

}} // namespace vigine::platform

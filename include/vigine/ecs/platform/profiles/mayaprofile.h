#pragma once
#include "vigine/ecs/platform/inputprofilecomponent.h"

namespace vigine { namespace platform {

class MayaProfile : public InputProfileComponent
{
  public:
    MayaProfile();
    const char* name() const override { return "Autodesk Maya"; }
    const char* description() const override { return "Maya-style: W/E/R for Move/Rotate/Scale, F for frame selected"; }
  protected:
    void populateBindings() override;
};

}} // namespace vigine::platform

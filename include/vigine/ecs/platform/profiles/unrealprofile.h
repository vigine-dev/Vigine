#pragma once
#include "vigine/ecs/platform/inputprofilecomponent.h"

namespace vigine { namespace platform {

class UnrealProfile : public InputProfileComponent
{
  public:
    UnrealProfile();
    const char* name() const override { return "Unreal Engine"; }
    const char* description() const override { return "Unreal Engine-style: W/E/R for Move/Rotate/Scale, Ctrl+W duplicate"; }
  protected:
    void populateBindings() override;
};

}} // namespace vigine::platform

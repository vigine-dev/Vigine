#pragma once
#include "vigine/ecs/platform/inputprofilecomponent.h"

namespace vigine { namespace platform {

class GodotProfile : public InputProfileComponent
{
  public:
    GodotProfile();
    const char* name() const override { return "Godot Engine"; }
    const char* description() const override { return "Godot-style: W/E/R for Move/Rotate/Scale, F for frame selected"; }
  protected:
    void populateBindings() override;
};

}} // namespace vigine::platform

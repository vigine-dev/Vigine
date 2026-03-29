#pragma once
#include "vigine/ecs/platform/inputprofilecomponent.h"

namespace vigine { namespace platform {

class BlenderClassicProfile : public InputProfileComponent
{
  public:
    BlenderClassicProfile();
    const char* name() const override { return "Blender Classic"; }
    const char* description() const override { return "Blender-style: G for Grab, RMB select"; }
  protected:
    void populateBindings() override;
};

}} // namespace vigine::platform

#pragma once
#include "vigine/ecs/platform/inputprofilecomponent.h"

namespace vigine
{
namespace platform
{

class BlenderModernProfile : public InputProfileComponent
{
  public:
    BlenderModernProfile();
    const char *name() const override { return "Blender Modern (3.x)"; }
    const char *description() const override { return "Blender 3.x-style: G for Grab, LMB select"; }

  protected:
    void populateBindings() override;
};

} // namespace platform
} // namespace vigine

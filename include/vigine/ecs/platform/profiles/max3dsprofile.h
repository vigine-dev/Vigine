#pragma once
#include "vigine/ecs/platform/inputprofilecomponent.h"

namespace vigine { namespace platform {

class Max3dsProfile : public InputProfileComponent
{
  public:
    Max3dsProfile();
    const char* name() const override { return "Autodesk 3ds Max"; }
    const char* description() const override { return "3ds Max-style: W/E/R for Move/Rotate/Scale"; }
  protected:
    void populateBindings() override;
};

}} // namespace vigine::platform

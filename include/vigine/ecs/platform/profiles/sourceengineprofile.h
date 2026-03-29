#pragma once
#include "vigine/ecs/platform/inputprofilecomponent.h"

namespace vigine { namespace platform {

class SourceEngineProfile : public InputProfileComponent
{
  public:
    SourceEngineProfile();
    const char* name() const override { return "Source Engine (Hammer)"; }
    const char* description() const override { return "Hammer Editor-style: Shift+W/E/R for Move/Rotate/Scale"; }
  protected:
    void populateBindings() override;
};

}} // namespace vigine::platform

#include "vigine/ecs/platform/profiles/sourceengineprofile.h"

#include "vigine/ecs/platform/iwindoweventhandler.h"


namespace vigine
{
namespace platform
{

namespace vk
{
constexpr unsigned int W = 0x57;
} // namespace vk

SourceEngineProfile::SourceEngineProfile() { populateBindings(); }

void SourceEngineProfile::populateBindings()
{
    // Hammer Editor: Shift+W for grab
    _map.addBinding(InputAction::GrabMode, {vk::W, KeyModifierShift});
}

} // namespace platform
} // namespace vigine

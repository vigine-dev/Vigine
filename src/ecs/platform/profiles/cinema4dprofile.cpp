#include "vigine/ecs/platform/profiles/cinema4dprofile.h"

#include "vigine/ecs/platform/iwindoweventhandler.h"


namespace vigine
{
namespace platform
{

namespace vk
{
constexpr unsigned int E = 0x45;
constexpr unsigned int S = 0x53;
} // namespace vk

Cinema4DProfile::Cinema4DProfile() { populateBindings(); }

void Cinema4DProfile::populateBindings()
{
    // Cinema 4D: E for grab, S for frame selected
    _map.addBinding(InputAction::GrabMode, {vk::E});
    _map.addBinding(InputAction::FrameSelected, {vk::S});
}

} // namespace platform
} // namespace vigine

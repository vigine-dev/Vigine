#include "vigine/ecs/platform/profiles/unityprofile.h"

#include "vigine/ecs/platform/iwindoweventhandler.h"


namespace vigine
{
namespace platform
{

namespace vk
{
constexpr unsigned int D = 0x44;
constexpr unsigned int F = 0x46;
constexpr unsigned int W = 0x57;
} // namespace vk

UnityProfile::UnityProfile() { populateBindings(); }

void UnityProfile::populateBindings()
{
    // Unity: W for grab, Ctrl+D duplicate, F frame selected
    _map.addBinding(InputAction::GrabMode, {vk::W});
    _map.addBinding(InputAction::Duplicate, {vk::D, KeyModifierControl});
    _map.addBinding(InputAction::FrameSelected, {vk::F});
}

} // namespace platform
} // namespace vigine

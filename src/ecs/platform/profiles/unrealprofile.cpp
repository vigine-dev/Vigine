#include "vigine/ecs/platform/profiles/unrealprofile.h"

#include "vigine/ecs/platform/iwindoweventhandler.h"


namespace vigine
{
namespace platform
{

namespace vk
{
constexpr unsigned int W = 0x57;
} // namespace vk

UnrealProfile::UnrealProfile() { populateBindings(); }

void UnrealProfile::populateBindings()
{
    // Unreal: W for grab, Ctrl+W duplicate
    _map.addBinding(InputAction::GrabMode, {vk::W});
    _map.addBinding(InputAction::Duplicate, {vk::W, KeyModifierControl});
}

} // namespace platform
} // namespace vigine

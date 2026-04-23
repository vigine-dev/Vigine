#include "vigine/ecs/platform/profiles/godotprofile.h"

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

GodotProfile::GodotProfile() { populateBindings(); }

void GodotProfile::populateBindings()
{
    // Godot: W for grab, F for frame selected, Shift+D duplicate
    _map.addBinding(InputAction::GrabMode, {vk::W});
    _map.addBinding(InputAction::FrameSelected, {vk::F});
    _map.addBinding(InputAction::Duplicate, {vk::D, KeyModifierShift});
}

} // namespace platform
} // namespace vigine

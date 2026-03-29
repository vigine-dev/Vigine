#include "vigine/ecs/platform/profiles/blenderclassicprofile.h"

#include "vigine/ecs/platform/iwindoweventhandler.h"


namespace vigine
{
namespace platform
{

namespace vk
{
constexpr unsigned int D = 0x44;
constexpr unsigned int G = 0x47;
} // namespace vk

BlenderClassicProfile::BlenderClassicProfile() { populateBindings(); }

void BlenderClassicProfile::populateBindings()
{
    // Blender Classic: G for grab, Shift+D duplicate
    _map.addBinding(InputAction::GrabMode, {vk::G});
    _map.addBinding(InputAction::Duplicate, {vk::D, KeyModifierShift});
}

} // namespace platform
} // namespace vigine

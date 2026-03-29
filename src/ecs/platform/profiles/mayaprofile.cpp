#include "vigine/ecs/platform/profiles/mayaprofile.h"

#include "vigine/ecs/platform/iwindoweventhandler.h"


namespace vigine
{
namespace platform
{

namespace vk
{
constexpr unsigned int A = 0x41;
constexpr unsigned int F = 0x46;
constexpr unsigned int W = 0x57;
} // namespace vk

MayaProfile::MayaProfile() { populateBindings(); }

void MayaProfile::populateBindings()
{
    // Maya: W for grab, F for frame selected, A for frame all
    _map.addBinding(InputAction::GrabMode, {vk::W});
    _map.addBinding(InputAction::FrameSelected, {vk::F});
    _map.addBinding(InputAction::FrameAll, {vk::A});
}

} // namespace platform
} // namespace vigine

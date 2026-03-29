#include "vigine/ecs/platform/profiles/max3dsprofile.h"

#include "vigine/ecs/platform/iwindoweventhandler.h"


namespace vigine
{
namespace platform
{

namespace vk
{
constexpr unsigned int W = 0x57;
} // namespace vk

Max3dsProfile::Max3dsProfile() { populateBindings(); }

void Max3dsProfile::populateBindings()
{
    // 3ds Max: W for grab
    _map.addBinding(InputAction::GrabMode, {vk::W});
}

} // namespace platform
} // namespace vigine

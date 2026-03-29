#pragma once

#include "vigine/ecs/platform/inputaction.h"
#include "vigine/ecs/platform/iwindoweventhandler.h"

#include <optional>
#include <unordered_map>
#include <vector>

namespace vigine
{
namespace platform
{

struct InputBinding
{
    unsigned int keyCode{0};
    unsigned int modifiers{KeyModifierNone};
    std::optional<MouseButton> mouseButton{};
};

class InputMap
{
  public:
    // Add a binding for an action.
    void addBinding(InputAction action, InputBinding binding);

    // Find all actions matching a key press (after applying emulation remapping).
    std::vector<InputAction> findActions(unsigned int keyCode, unsigned int modifiers) const;

    // When enabled, top-row digit keys (0-9, -, =, .) are remapped to
    // their Numpad VK equivalents before lookup.
    void setNumpadEmulation(bool enabled);
    bool numpadEmulation() const;

    // When enabled, the consumer should treat Alt+LMB as MMB (EmulateMMB action).
    // Check this flag when handling mouse button events.
    void setEmulate3ButtonMouse(bool enabled);
    bool emulate3ButtonMouse() const;

    // Build the default Blender-style key map.
    [[deprecated("Use InputProfileComponent (BlenderClassicProfile) instead")]]
    static InputMap createDefaultMap();

  private:
    static unsigned int applyNumpadEmulation(unsigned int keyCode);

    std::unordered_map<InputAction, std::vector<InputBinding>> _bindings;
    bool _numpadEmulation{false};
    bool _emulate3ButtonMouse{false};
};

} // namespace platform
} // namespace vigine

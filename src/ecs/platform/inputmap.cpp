#include "vigine/ecs/platform/inputmap.h"

namespace vigine
{
namespace platform
{

// ─── VK constants (shared by WinAPI and macOS/Linux layers) ─────────────────
namespace vk
{
// Letters
constexpr unsigned int A = 0x41;
constexpr unsigned int D = 0x44;
constexpr unsigned int E = 0x45;
constexpr unsigned int G = 0x47;
constexpr unsigned int Q = 0x51;
constexpr unsigned int R = 0x52;
constexpr unsigned int S = 0x53;
constexpr unsigned int W = 0x57;
constexpr unsigned int X = 0x58;
constexpr unsigned int Y = 0x59;
constexpr unsigned int Z = 0x5A;
constexpr unsigned int C = 0x43;

// Arrow keys
constexpr unsigned int Left  = 0x25;
constexpr unsigned int Up    = 0x26;
constexpr unsigned int Right = 0x27;
constexpr unsigned int Down  = 0x28;

// Special keys
constexpr unsigned int Home    = 0x24;
constexpr unsigned int Delete  = 0x2E;
constexpr unsigned int Return  = 0x0D;
constexpr unsigned int Escape  = 0x1B;
constexpr unsigned int Tab     = 0x09;
constexpr unsigned int Shift   = 0x10;
constexpr unsigned int Control = 0x11;

// Numpad
constexpr unsigned int Numpad0        = 0x60;
constexpr unsigned int Numpad2        = 0x62;
constexpr unsigned int Numpad4        = 0x64;
constexpr unsigned int Numpad5        = 0x65;
constexpr unsigned int Numpad6        = 0x66;
constexpr unsigned int Numpad8        = 0x68;
constexpr unsigned int NumpadAdd      = 0x6B;
constexpr unsigned int NumpadSubtract = 0x6D;
constexpr unsigned int NumpadDecimal  = 0x6E;

// OEM
constexpr unsigned int OemComma  = 0xBC; // ','
constexpr unsigned int OemMinus  = 0xBD; // '-'
constexpr unsigned int OemPlus   = 0xBB; // '='
constexpr unsigned int OemPeriod = 0xBE; // '.'
} // namespace vk

// ─── InputMap ────────────────────────────────────────────────────────────────

void InputMap::addBinding(InputAction action, InputBinding binding)
{
    _bindings[action].push_back(binding);
}

std::vector<InputAction> InputMap::findActions(unsigned int keyCode, unsigned int modifiers) const
{
    if (_numpadEmulation)
        keyCode = applyNumpadEmulation(keyCode);

    // Ignore Caps/Num lock state when matching bindings
    constexpr unsigned int relevantMods =
        KeyModifierShift | KeyModifierControl | KeyModifierAlt | KeyModifierSuper;
    const unsigned int normalizedMods = modifiers & relevantMods;

    std::vector<InputAction> result;
    for (const auto &[action, bindings] : _bindings)
    {
        for (const auto &b : bindings)
        {
            const unsigned int bindingMods = b.modifiers & relevantMods;
            if (b.keyCode == keyCode && bindingMods == normalizedMods)
            {
                result.push_back(action);
                break;
            }
        }
    }
    return result;
}

void InputMap::setNumpadEmulation(bool enabled) { _numpadEmulation = enabled; }
bool InputMap::numpadEmulation() const { return _numpadEmulation; }

void InputMap::setEmulate3ButtonMouse(bool enabled) { _emulate3ButtonMouse = enabled; }
bool InputMap::emulate3ButtonMouse() const { return _emulate3ButtonMouse; }

// static
unsigned int InputMap::applyNumpadEmulation(unsigned int keyCode)
{
    // Top-row digits 0-9 (0x30-0x39) → VK_NUMPAD0-9 (0x60-0x69)
    if (keyCode >= 0x30 && keyCode <= 0x39)
        return keyCode + 0x30u;
    // '-' (VK_OEM_MINUS) → VK_SUBTRACT
    if (keyCode == vk::OemMinus)
        return vk::NumpadSubtract;
    // '=' (VK_OEM_PLUS) → VK_ADD
    if (keyCode == vk::OemPlus)
        return vk::NumpadAdd;
    // '.' (VK_OEM_PERIOD) → VK_DECIMAL
    if (keyCode == vk::OemPeriod)
        return vk::NumpadDecimal;
    return keyCode;
}

// static
InputMap InputMap::createDefaultMap()
{
    InputMap map;

    const auto bind = [&](InputAction action, unsigned int key,
                          unsigned int mods = KeyModifierNone) {
        map.addBinding(action, {key, mods});
    };

    // ── Camera Movement ──────────────────────────────────────────────────────
    bind(InputAction::MoveForward, vk::W);
    bind(InputAction::MoveBackward, vk::S);
    bind(InputAction::MoveLeft, vk::A);
    bind(InputAction::MoveRight, vk::D);
    bind(InputAction::MoveUp, vk::E);
    bind(InputAction::MoveDown, vk::Q);
    // Arrow key duplicates (Up/Down = forward/backward, Left/Right = strafe)
    bind(InputAction::MoveLeft, vk::Left);
    bind(InputAction::MoveRight, vk::Right);
    bind(InputAction::MoveForward, vk::Up);
    bind(InputAction::MoveBackward, vk::Down);

    // ── Discrete Rotation (Numpad) ───────────────────────────────────────────
    bind(InputAction::RotateYawLeft, vk::Numpad4);
    bind(InputAction::RotateYawRight, vk::Numpad6);
    bind(InputAction::RotatePitchUp, vk::Numpad8);
    bind(InputAction::RotatePitchDown, vk::Numpad2);

    // ── Zoom ─────────────────────────────────────────────────────────────────
    bind(InputAction::ZoomIn, vk::NumpadAdd);
    bind(InputAction::ZoomOut, vk::NumpadSubtract);

    // ── Camera Reset / Frame ─────────────────────────────────────────────────
    bind(InputAction::CameraView, vk::Numpad0);
    bind(InputAction::ResetView, vk::C, KeyModifierShift);
    bind(InputAction::ResetRotation, vk::R, KeyModifierAlt);
    bind(InputAction::ResetPosition, vk::G, KeyModifierAlt);
    bind(InputAction::FrameSelected, vk::NumpadDecimal);
    bind(InputAction::FrameAll, vk::Home);

    // ── Speed Modifiers ──────────────────────────────────────────────────────
    bind(InputAction::SpeedSlow, vk::Shift);
    bind(InputAction::SpeedFast, vk::Control);

    // ── Selection ────────────────────────────────────────────────────────────
    bind(InputAction::SelectAll, vk::A);
    bind(InputAction::DeselectAll, vk::A, KeyModifierAlt);

    // ── Transform Modes ──────────────────────────────────────────────────────
    bind(InputAction::GrabMode, vk::G);
    bind(InputAction::ConstrainX, vk::X);
    bind(InputAction::ConstrainY, vk::Y);
    bind(InputAction::ConstrainZ, vk::Z);

    // ── Object Actions ───────────────────────────────────────────────────────
    bind(InputAction::Duplicate, vk::D, KeyModifierShift);
    bind(InputAction::Delete, vk::X);
    bind(InputAction::Delete, vk::Delete);

    // ── Confirmation ─────────────────────────────────────────────────────────
    bind(InputAction::Confirm, vk::Return);
    bind(InputAction::Cancel, vk::Escape);

    return map;
}

} // namespace platform
} // namespace vigine

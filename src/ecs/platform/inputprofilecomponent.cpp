#include "vigine/ecs/platform/inputprofilecomponent.h"

#include "vigine/ecs/platform/iwindoweventhandler.h"

namespace vigine
{
namespace platform
{

namespace vk
{
constexpr unsigned int A              = 0x41;
constexpr unsigned int D              = 0x44;
constexpr unsigned int E              = 0x45;
constexpr unsigned int G              = 0x47;
constexpr unsigned int Q              = 0x51;
constexpr unsigned int R              = 0x52;
constexpr unsigned int S              = 0x53;
constexpr unsigned int W              = 0x57;
constexpr unsigned int X              = 0x58;
constexpr unsigned int Y              = 0x59;
constexpr unsigned int Z              = 0x5A;
constexpr unsigned int C              = 0x43;
constexpr unsigned int F10            = 0x79;
constexpr unsigned int Left           = 0x25;
constexpr unsigned int Up             = 0x26;
constexpr unsigned int Right          = 0x27;
constexpr unsigned int Down           = 0x28;
constexpr unsigned int Home           = 0x24;
constexpr unsigned int Delete         = 0x2E;
constexpr unsigned int Return         = 0x0D;
constexpr unsigned int Escape         = 0x1B;
constexpr unsigned int Tab            = 0x09;
constexpr unsigned int Shift          = 0x10;
constexpr unsigned int Control        = 0x11;
constexpr unsigned int Numpad0        = 0x60;
constexpr unsigned int Numpad2        = 0x62;
constexpr unsigned int Numpad4        = 0x64;
constexpr unsigned int Numpad5        = 0x65;
constexpr unsigned int Numpad6        = 0x66;
constexpr unsigned int Numpad8        = 0x68;
constexpr unsigned int NumpadAdd      = 0x6B;
constexpr unsigned int NumpadSubtract = 0x6D;
constexpr unsigned int NumpadDecimal  = 0x6E;
constexpr unsigned int OemComma       = 0xBC;
constexpr unsigned int OemMinus       = 0xBD;
constexpr unsigned int OemPlus        = 0xBB;
constexpr unsigned int OemPeriod      = 0xBE;
} // namespace vk

InputProfileComponent::InputProfileComponent()
{
    addCommonBindings();
    // NOTE: populateBindings() is NOT called here — it is pure virtual.
    // Each concrete subclass must call populateBindings() from its own constructor.
}

void InputProfileComponent::addCommonBindings()
{
    const auto bind = [this](InputAction action, unsigned int key,
                             unsigned int mods = KeyModifierNone) {
        _map.addBinding(action, {key, mods});
    };

    // ── Camera Movement ──────────────────────────────────────────────────────
    bind(InputAction::MoveForward, vk::W);
    bind(InputAction::MoveBackward, vk::S);
    bind(InputAction::MoveLeft, vk::A);
    bind(InputAction::MoveRight, vk::D);
    bind(InputAction::MoveUp, vk::E);
    bind(InputAction::MoveDown, vk::Q);
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
    bind(InputAction::SpeedFast, vk::Shift);
    bind(InputAction::SpeedSlow, vk::Control);

    // ── Selection ────────────────────────────────────────────────────────────
    bind(InputAction::SelectAll, vk::A);
    bind(InputAction::DeselectAll, vk::A, KeyModifierAlt);

    // ── Axis Constraints ─────────────────────────────────────────────────────
    bind(InputAction::ConstrainX, vk::X);
    bind(InputAction::ConstrainY, vk::Y);
    bind(InputAction::ConstrainZ, vk::Z);

    // ── Object Actions ───────────────────────────────────────────────────────
    bind(InputAction::Delete, vk::X);
    bind(InputAction::Delete, vk::Delete);

    // ── Camera Mode ──────────────────────────────────────────────────────────
    bind(InputAction::ToggleCameraMode, vk::Numpad5);

    // ── Confirmation ─────────────────────────────────────────────────────────
    bind(InputAction::Confirm, vk::Return);
    bind(InputAction::Cancel, vk::Escape);

    // ── Edit History ─────────────────────────────────────────────────────────
    bind(InputAction::Undo, vk::Z, KeyModifierControl);
    bind(InputAction::Redo, vk::Z, KeyModifierControl | KeyModifierShift);

    // ── UI ───────────────────────────────────────────────────────────────────
    bind(InputAction::ToggleSettings, vk::F10);
}

} // namespace platform
} // namespace vigine

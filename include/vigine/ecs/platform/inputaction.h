#pragma once

namespace vigine
{
namespace platform
{

enum class InputAction
{
    // Camera Movement
    MoveForward,
    MoveBackward,
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,

    // Camera Rotation (discrete, Numpad)
    RotateYawLeft,
    RotateYawRight,
    RotatePitchUp,
    RotatePitchDown,

    // Camera Orbit
    OrbitYawLeft,
    OrbitYawRight,
    OrbitPitchUp,
    OrbitPitchDown,

    // Camera Zoom
    ZoomIn,
    ZoomOut,

    // Camera View / Reset
    CameraView,
    ResetView,
    ResetRotation,
    ResetPosition,

    // Camera Focus
    FrameSelected,
    FrameAll,

    // Speed Modifiers
    SpeedSlow,
    SpeedFast,

    // Selection
    SelectAll,
    DeselectAll,

    // Transform Modes
    GrabMode,

    // Axis Constraints
    ConstrainX,
    ConstrainY,
    ConstrainZ,

    // Object Actions
    Duplicate,
    Delete,

    // Camera Mode
    ToggleCameraMode,

    // Confirmation
    Confirm,
    Cancel,

    // Input Emulation
    EmulateMMB,

    // Edit History
    Undo,
    Redo,

    // UI
    ToggleSettings,
};

} // namespace platform
} // namespace vigine

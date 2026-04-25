#pragma once

/**
 * @file iwindoweventhandler.h
 * @brief Platform window-event handler interface and its input event types.
 */

namespace vigine
{
namespace platform
{
/**
 * @brief Mouse buttons reported by window events.
 */
enum class MouseButton
{
    Left,
    Right,
    Middle,
    X1,
    X2,
};

/**
 * @brief Bit-flag set describing active keyboard modifiers during an event.
 */
enum KeyModifier : unsigned int
{
    KeyModifierNone    = 0,
    KeyModifierShift   = 1u << 0,
    KeyModifierControl = 1u << 1,
    KeyModifierAlt     = 1u << 2,
    KeyModifierSuper   = 1u << 3,
    KeyModifierCaps    = 1u << 4,
    KeyModifierNum     = 1u << 5,
};

/**
 * @brief Snapshot of a single key press / release.
 */
struct KeyEvent
{
    unsigned int keyCode{0};
    unsigned int scanCode{0};
    unsigned int modifiers{KeyModifierNone};
    unsigned int repeatCount{0};
    bool isRepeat{false};
};

/**
 * @brief Snapshot of a single text-input event (one Unicode code point).
 */
struct TextEvent
{
    unsigned int codePoint{0};
    unsigned int modifiers{KeyModifierNone};
    unsigned int repeatCount{0};
};

/**
 * @brief Pure-interface callback for all window-surface input events.
 *
 * WindowSystem dispatches platform events to any registered handler
 * component. Implementers receive window lifecycle notifications,
 * mouse events (move / wheel / button / enter / leave), and keyboard
 * events (key down / up, character, dead-key).
 *
 * @note TODO(#293): The lower-level platform window primitive (window
 * creation, OS event-queue pumping, native-handle management) lives
 * on @c vigine::platform::IWindowSystem in
 * @c include/vigine/api/platform/iwindowsystem.h. This ECS-side
 * @c IWindowEventHandlerComponent is the consumer callback registered
 * against a specific window component once the platform side has
 * created the window. A follow-up leaf will reconcile the two layers.
 */
class IWindowEventHandlerComponent
{
  public:
    virtual ~IWindowEventHandlerComponent() = default;

    // Window lifecycle
    virtual void onWindowClosed()                       = 0;
    virtual void onWindowResized(int width, int height) = 0;
    virtual void onWindowMoved(int x, int y)            = 0;
    virtual void onWindowFocused()                      = 0;
    virtual void onWindowUnfocused()                    = 0;

    // Mouse events
    virtual void onMouseMove(int x, int y)                                  = 0;
    virtual void onMouseEnter()                                             = 0;
    virtual void onMouseLeave()                                             = 0;
    virtual void onMouseWheel(int delta, int x, int y)                      = 0;
    virtual void onMouseHorizontalWheel(int delta, int x, int y)            = 0;
    virtual void onMouseButtonDown(MouseButton button, int x, int y)        = 0;
    virtual void onMouseButtonUp(MouseButton button, int x, int y)          = 0;
    virtual void onMouseButtonDoubleClick(MouseButton button, int x, int y) = 0;

    // Keyboard events
    virtual void onKeyDown(const KeyEvent &event)   = 0;
    virtual void onKeyUp(const KeyEvent &event)     = 0;
    virtual void onChar(const TextEvent &event)     = 0;
    virtual void onDeadChar(const TextEvent &event) = 0;
};

} // namespace platform
} // namespace vigine

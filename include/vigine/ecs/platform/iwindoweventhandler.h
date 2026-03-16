#pragma once

namespace vigine
{
namespace platform
{
enum class MouseButton
{
    Left,
    Right,
    Middle,
    X1,
    X2,
};

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

struct KeyEvent
{
    unsigned int keyCode{0};
    unsigned int scanCode{0};
    unsigned int modifiers{KeyModifierNone};
    unsigned int repeatCount{0};
    bool isRepeat{false};
};

struct TextEvent
{
    unsigned int codePoint{0};
    unsigned int modifiers{KeyModifierNone};
    unsigned int repeatCount{0};
};

class IWindowEventHandler
{
  public:
    virtual ~IWindowEventHandler() = default;

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

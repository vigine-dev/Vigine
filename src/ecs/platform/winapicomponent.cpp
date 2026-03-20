#ifdef _WIN32

#include "winapicomponent.h"

#include "vigine/ecs/platform/iwindoweventhandler.h"

#include <windows.h>
#include <windowsx.h>

namespace
{
constexpr char kWindowClassName[] = "VigineWindowClass";

constexpr bool wasDownBefore(LPARAM lParam) { return (lParam & (1 << 30)) != 0; }

constexpr unsigned int keyRepeatCount(LPARAM lParam)
{
    return static_cast<unsigned int>(LOWORD(lParam));
}

constexpr unsigned int keyScanCode(LPARAM lParam)
{
    return static_cast<unsigned int>((lParam >> 16) & 0xFFu);
}

unsigned int queryModifiers()
{
    using namespace vigine::platform;

    unsigned int modifiers = KeyModifierNone;
    if (GetKeyState(VK_SHIFT) & 0x8000)
        modifiers |= KeyModifierShift;
    if (GetKeyState(VK_CONTROL) & 0x8000)
        modifiers |= KeyModifierControl;
    if (GetKeyState(VK_MENU) & 0x8000)
        modifiers |= KeyModifierAlt;
    if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000))
        modifiers |= KeyModifierSuper;
    if (GetKeyState(VK_CAPITAL) & 0x0001)
        modifiers |= KeyModifierCaps;
    if (GetKeyState(VK_NUMLOCK) & 0x0001)
        modifiers |= KeyModifierNum;

    return modifiers;
}

void ensureMouseLeaveTracking(HWND hwnd, vigine::platform::WinAPIComponent *pThis)
{
    auto *eventHandler = pThis ? pThis->_eventHandler : nullptr;

    if (!pThis || pThis->isMouseTracking())
        return;

    TRACKMOUSEEVENT trackingInfo{};
    trackingInfo.cbSize      = sizeof(trackingInfo);
    trackingInfo.dwFlags     = TME_LEAVE;
    trackingInfo.hwndTrack   = hwnd;
    trackingInfo.dwHoverTime = HOVER_DEFAULT;
    if (TrackMouseEvent(&trackingInfo))
    {
        pThis->setMouseTracking(true);
        if (eventHandler)
            eventHandler->onMouseEnter();
    }
}

void dispatchMouseButtonDown(vigine::platform::WinAPIComponent *pThis,
                             vigine::platform::MouseButton button, LPARAM lParam)
{
    auto *eventHandler = pThis ? pThis->_eventHandler : nullptr;
    if (!eventHandler)
        return;

    eventHandler->onMouseButtonDown(button, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
}

void dispatchMouseButtonUp(vigine::platform::WinAPIComponent *pThis,
                           vigine::platform::MouseButton button, LPARAM lParam)
{
    auto *eventHandler = pThis ? pThis->_eventHandler : nullptr;
    if (!eventHandler)
        return;

    eventHandler->onMouseButtonUp(button, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
}

void dispatchMouseButtonDoubleClick(vigine::platform::WinAPIComponent *pThis,
                                    vigine::platform::MouseButton button, LPARAM lParam)
{
    auto *eventHandler = pThis ? pThis->_eventHandler : nullptr;
    if (!eventHandler)
        return;

    eventHandler->onMouseButtonDoubleClick(button, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    vigine::platform::WinAPIComponent *pThis = nullptr;

    if (message == WM_CREATE)
    {
        CREATESTRUCTA *pCreate = reinterpret_cast<CREATESTRUCTA *>(lParam);
        pThis = reinterpret_cast<vigine::platform::WinAPIComponent *>(pCreate->lpCreateParams);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else
    {
        pThis = reinterpret_cast<vigine::platform::WinAPIComponent *>(
            GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    }

    auto *eventHandler = pThis ? pThis->_eventHandler : nullptr;

    if (!pThis || !eventHandler)
    {
        if (message == WM_DESTROY)
        {
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcA(hwnd, message, wParam, lParam);
    }

    switch (message)
    {
    case WM_DESTROY:
        eventHandler->onWindowClosed();
        PostQuitMessage(0);
        return 0;
    case WM_SIZE: {
        const int width  = GET_X_LPARAM(lParam);
        const int height = GET_Y_LPARAM(lParam);
        eventHandler->onWindowResized(width, height);
        return 0;
    }
    case WM_MOVE: {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        eventHandler->onWindowMoved(x, y);
        return 0;
    }
    case WM_SETFOCUS:
        eventHandler->onWindowFocused();
        return 0;
    case WM_KILLFOCUS:
        eventHandler->onWindowUnfocused();
        return 0;
    case WM_MOUSEMOVE:
        ensureMouseLeaveTracking(hwnd, pThis);
        eventHandler->onMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSELEAVE:
        pThis->setMouseTracking(false);
        eventHandler->onMouseLeave();
        return 0;
    case WM_MOUSEWHEEL:
        eventHandler->onMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam), GET_X_LPARAM(lParam),
                                   GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSEHWHEEL:
        eventHandler->onMouseHorizontalWheel(GET_WHEEL_DELTA_WPARAM(wParam), GET_X_LPARAM(lParam),
                                             GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONDOWN:
        dispatchMouseButtonDown(pThis, vigine::platform::MouseButton::Left, lParam);
        return 0;
    case WM_LBUTTONUP:
        dispatchMouseButtonUp(pThis, vigine::platform::MouseButton::Left, lParam);
        return 0;
    case WM_LBUTTONDBLCLK:
        dispatchMouseButtonDoubleClick(pThis, vigine::platform::MouseButton::Left, lParam);
        return 0;
    case WM_RBUTTONDOWN:
        dispatchMouseButtonDown(pThis, vigine::platform::MouseButton::Right, lParam);
        return 0;
    case WM_RBUTTONUP:
        dispatchMouseButtonUp(pThis, vigine::platform::MouseButton::Right, lParam);
        return 0;
    case WM_RBUTTONDBLCLK:
        dispatchMouseButtonDoubleClick(pThis, vigine::platform::MouseButton::Right, lParam);
        return 0;
    case WM_MBUTTONDOWN:
        dispatchMouseButtonDown(pThis, vigine::platform::MouseButton::Middle, lParam);
        return 0;
    case WM_MBUTTONUP:
        dispatchMouseButtonUp(pThis, vigine::platform::MouseButton::Middle, lParam);
        return 0;
    case WM_MBUTTONDBLCLK:
        dispatchMouseButtonDoubleClick(pThis, vigine::platform::MouseButton::Middle, lParam);
        return 0;
    case WM_XBUTTONDOWN:
        dispatchMouseButtonDown(pThis,
                                GET_XBUTTON_WPARAM(wParam) == XBUTTON1
                                    ? vigine::platform::MouseButton::X1
                                    : vigine::platform::MouseButton::X2,
                                lParam);
        return TRUE;
    case WM_XBUTTONUP:
        dispatchMouseButtonUp(pThis,
                              GET_XBUTTON_WPARAM(wParam) == XBUTTON1
                                  ? vigine::platform::MouseButton::X1
                                  : vigine::platform::MouseButton::X2,
                              lParam);
        return TRUE;
    case WM_XBUTTONDBLCLK:
        dispatchMouseButtonDoubleClick(pThis,
                                       GET_XBUTTON_WPARAM(wParam) == XBUTTON1
                                           ? vigine::platform::MouseButton::X1
                                           : vigine::platform::MouseButton::X2,
                                       lParam);
        return TRUE;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        eventHandler->onKeyDown(vigine::platform::KeyEvent{
            .keyCode     = static_cast<unsigned int>(wParam),
            .scanCode    = keyScanCode(lParam),
            .modifiers   = queryModifiers(),
            .repeatCount = keyRepeatCount(lParam),
            .isRepeat    = wasDownBefore(lParam),
        });
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        eventHandler->onKeyUp(vigine::platform::KeyEvent{
            .keyCode     = static_cast<unsigned int>(wParam),
            .scanCode    = keyScanCode(lParam),
            .modifiers   = queryModifiers(),
            .repeatCount = keyRepeatCount(lParam),
            .isRepeat    = false,
        });
        return 0;
    case WM_CHAR:
    case WM_SYSCHAR:
        eventHandler->onChar(vigine::platform::TextEvent{
            .codePoint   = static_cast<unsigned int>(wParam),
            .modifiers   = queryModifiers(),
            .repeatCount = keyRepeatCount(lParam),
        });
        return 0;
    case WM_DEADCHAR:
    case WM_SYSDEADCHAR:
        eventHandler->onDeadChar(vigine::platform::TextEvent{
            .codePoint   = static_cast<unsigned int>(wParam),
            .modifiers   = queryModifiers(),
            .repeatCount = keyRepeatCount(lParam),
        });
        return 0;
    default:
        return DefWindowProcA(hwnd, message, wParam, lParam);
    }
}
} // namespace

vigine::platform::WinAPIComponent *vigine::platform::WinAPIComponent::_instance = nullptr;

void vigine::platform::WinAPIComponent::show()
{
    _instance                = this;

    const HINSTANCE instance = GetModuleHandleA(nullptr);

    WNDCLASSEXA windowClass{};
    windowClass.cbSize        = sizeof(windowClass);
    windowClass.style         = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc   = windowProc;
    windowClass.hInstance     = instance;
    windowClass.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kWindowClassName;

    const ATOM classAtom      = RegisterClassExA(&windowClass);
    if (classAtom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return;

    HWND window =
        CreateWindowExA(0, kWindowClassName, "Vigine Window", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                        CW_USEDEFAULT, 940, 660, nullptr, nullptr, instance, this);
    if (!window)
        return;

    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageA(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
}

bool vigine::platform::WinAPIComponent::isMouseTracking() const { return _isMouseTracking; }

void vigine::platform::WinAPIComponent::setMouseTracking(bool value) { _isMouseTracking = value; }

#endif // _WIN32

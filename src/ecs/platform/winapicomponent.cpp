#ifdef _WIN32

#include "winapicomponent.h"

#include "vigine/ecs/platform/iwindoweventhandler.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <windows.h>
#include <windowsx.h>

namespace
{
constexpr char kWindowClassName[] = "VigineWindowClass";
constexpr int kOverlayWidth       = 340;
constexpr int kOverlayHeight      = 92;

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

std::string queryGpuNameForWindow(HWND hwnd)
{
    if (!hwnd)
        return "Unknown";

    const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXA monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfoA(monitor, &monitorInfo))
        return "Unknown";

    DISPLAY_DEVICEA adapter{};
    adapter.cb = sizeof(adapter);
    for (DWORD index = 0; EnumDisplayDevicesA(nullptr, index, &adapter, 0); ++index)
    {
        if ((adapter.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) == 0)
            continue;

        if (std::strcmp(adapter.DeviceName, monitorInfo.szDevice) == 0)
        {
            if (adapter.DeviceString[0] == '\0')
                return "Unknown";

            return adapter.DeviceString;
        }
    }

    return "Unknown";
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

void runFrameCallback(vigine::platform::WinAPIComponent *pThis)
{
    if (!pThis)
        return;

    pThis->runFrame();
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
        pThis->updateFpsOverlayPosition();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        runFrameCallback(pThis);
        return 0;
    }
    case WM_MOVE: {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        eventHandler->onWindowMoved(x, y);
        pThis->updateFpsOverlayPosition();
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
        if (wParam == VK_F3 && !wasDownBefore(lParam))
        {
            pThis->toggleOverlayVisibility();
            return 0;
        }

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

bool vigine::platform::WinAPIComponent::ensureWindowCreated()
{
    if (_windowHandle)
        return true;

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
        return false;

    _windowHandle =
        CreateWindowExA(0, kWindowClassName, "Vigine Window", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                        CW_USEDEFAULT, 940, 660, nullptr, nullptr, instance, this);

    if (_windowHandle)
        createFpsOverlay();

    return _windowHandle != nullptr;
}

void vigine::platform::WinAPIComponent::createFpsOverlay()
{
    if (!_windowHandle || _fpsLabelHandle)
        return;

    _fpsLabelHandle =
        CreateWindowExA(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE, "STATIC",
                        _overlayText.data(), WS_POPUP | WS_VISIBLE | SS_LEFT, 0, 0, kOverlayWidth,
                        kOverlayHeight, _windowHandle, nullptr, GetModuleHandleA(nullptr), nullptr);

    if (_fpsLabelHandle)
    {
        SendMessageA(_fpsLabelHandle, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        updateFpsOverlayPosition();
        SetWindowPos(_fpsLabelHandle, HWND_TOPMOST, 0, 0, kOverlayWidth, kOverlayHeight,
                     SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    const auto gpuName = queryGpuNameForWindow(_windowHandle);
    std::snprintf(_gpuName.data(), _gpuName.size(), "%s", gpuName.c_str());

    _fpsSampleStart = std::chrono::steady_clock::now();
    _fpsFrameCount  = 0;
}

void vigine::platform::WinAPIComponent::updateFpsOverlay()
{
    ++_fpsFrameCount;
    const auto now     = std::chrono::steady_clock::now();
    const auto elapsed = now - _fpsSampleStart;
    if (elapsed < std::chrono::milliseconds(500))
        return;

    const auto elapsedMs =
        static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
    if (elapsedMs <= 0.0)
        return;

    const double fps     = static_cast<double>(_fpsFrameCount) * 1000.0 / elapsedMs;
    const double frameMs = elapsedMs / static_cast<double>((std::max)(_fpsFrameCount, 1u));

    RECT clientRect{};
    GetClientRect(_windowHandle, &clientRect);
    const int clientWidth  = clientRect.right - clientRect.left;
    const int clientHeight = clientRect.bottom - clientRect.top;

    int refreshHz          = 0;
    HDC dc                 = GetDC(_windowHandle);
    if (dc)
    {
        refreshHz = GetDeviceCaps(dc, VREFRESH);
        ReleaseDC(_windowHandle, dc);
    }

    const auto gpuName = queryGpuNameForWindow(_windowHandle);
    std::snprintf(_gpuName.data(), _gpuName.size(), "%s", gpuName.c_str());

    std::snprintf(_fpsText.data(), _fpsText.size(), "FPS: %.1f", fps);
    std::snprintf(_overlayText.data(), _overlayText.size(),
                  "FPS: %.1f (%.2f ms)\r\nGPU: %s\r\nMonitor: %d Hz\r\nWindow: %dx%d", fps, frameMs,
                  _gpuName.data(), refreshHz, clientWidth, clientHeight);

    if (_fpsLabelHandle && _overlayVisible)
    {
        SetWindowTextA(_fpsLabelHandle, _overlayText.data());
        updateFpsOverlayPosition();
    }

    char titleText[128]{};
    std::snprintf(titleText, sizeof(titleText), "Vigine Window | %s", _fpsText.data());
    if (_windowHandle)
        SetWindowTextA(_windowHandle, titleText);

    _fpsSampleStart = now;
    _fpsFrameCount  = 0;
}

void vigine::platform::WinAPIComponent::updateFpsOverlayPosition()
{
    if (!_windowHandle || !_fpsLabelHandle)
        return;

    POINT topLeft{8, 8};
    ClientToScreen(_windowHandle, &topLeft);
    SetWindowPos(_fpsLabelHandle, HWND_TOPMOST, topLeft.x, topLeft.y, kOverlayWidth, kOverlayHeight,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void vigine::platform::WinAPIComponent::toggleOverlayVisibility()
{
    _overlayVisible = !_overlayVisible;
    if (_fpsLabelHandle)
        ShowWindow(_fpsLabelHandle, _overlayVisible ? SW_SHOWNA : SW_HIDE);
}

void vigine::platform::WinAPIComponent::runFrame()
{
    runFrameCallback();
    updateFpsOverlay();
    updateFpsOverlayPosition();
}

void vigine::platform::WinAPIComponent::show()
{
    _instance = this;

    if (!ensureWindowCreated())
        return;

    ShowWindow(_windowHandle, SW_SHOW);
    UpdateWindow(_windowHandle);

    constexpr auto kTargetFrameTime = std::chrono::nanoseconds(6944444);
    auto nextFrameTime              = std::chrono::steady_clock::now();

    MSG message{};
    bool running = true;
    while (running)
    {
        while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                running = false;
                break;
            }

            TranslateMessage(&message);
            DispatchMessageA(&message);
        }

        if (!running)
            break;

        runFrame();

        nextFrameTime += kTargetFrameTime;
        auto now       = std::chrono::steady_clock::now();
        if (now < nextFrameTime)
        {
            // Coarse sleep first, then short yield-spin for better pacing precision.
            const auto coarseSleep =
                std::chrono::duration_cast<std::chrono::milliseconds>(nextFrameTime - now) -
                std::chrono::milliseconds(1);
            if (coarseSleep > std::chrono::milliseconds(0))
                std::this_thread::sleep_for(coarseSleep);

            while (std::chrono::steady_clock::now() < nextFrameTime)
                std::this_thread::yield();
        } else
        {
            // If frame took too long, reset deadline to avoid drift accumulation.
            nextFrameTime = now;
        }
    }
}

bool vigine::platform::WinAPIComponent::isMouseTracking() const { return _isMouseTracking; }

void vigine::platform::WinAPIComponent::setMouseTracking(bool value) { _isMouseTracking = value; }

void *vigine::platform::WinAPIComponent::nativeHandle() const
{
    auto *self = const_cast<WinAPIComponent *>(this);
    if (!self->ensureWindowCreated())
        return nullptr;

    return reinterpret_cast<void *>(_windowHandle);
}

#endif // _WIN32

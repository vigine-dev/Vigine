#ifdef _WIN32

#include "winapicomponent.h"

#include <windows.h>

namespace
{
constexpr char kWindowClassName[] = "VigineWindowClass";

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcA(hwnd, message, wParam, lParam);
    }
}
} // namespace

void vigine::platform::WinAPIComponent::show()
{
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
                        CW_USEDEFAULT, 960, 540, nullptr, nullptr, instance, nullptr);
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

#endif // _WIN32

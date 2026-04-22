#if defined(_WIN32)

#include "iossignalsource_win.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace vigine::eventscheduler
{

// Global singleton pointer — SetConsoleCtrlHandler requires a free function.
// The instance is set once when WinOsSignalSource is constructed and cleared
// when it is destroyed. The engine instantiates exactly one WinOsSignalSource.
static WinOsSignalSource *g_instance = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

namespace
{

[[nodiscard]] OsSignal mapWindowsCtrl(DWORD ctrlType) noexcept
{
    switch (ctrlType)
    {
        case CTRL_C_EVENT:        return OsSignal::Interrupt;
        case CTRL_CLOSE_EVENT:    return OsSignal::Terminate;
        // CTRL_BREAK_EVENT maps to Hangup (best-effort).
        case CTRL_BREAK_EVENT:    return OsSignal::Hangup;
        default:                  return OsSignal::Terminate;  // fallback
    }
}

BOOL WINAPI consoleCtrlHandler(DWORD ctrlType) noexcept
{
    WinOsSignalSource *src = g_instance;
    if (!src)
    {
        return FALSE;
    }
    OsSignal sig = mapWindowsCtrl(ctrlType);
    src->dispatch(sig);
    // Return FALSE to allow the default handler to run for CTRL_C
    // (letting the process exit naturally); return TRUE for CTRL_CLOSE
    // to suppress the default close behaviour so the engine can shut
    // down cleanly.
    return (ctrlType == CTRL_CLOSE_EVENT) ? TRUE : FALSE;
}

} // namespace

WinOsSignalSource::WinOsSignalSource()
{
    g_instance = this;
    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
}

WinOsSignalSource::~WinOsSignalSource()
{
    SetConsoleCtrlHandler(consoleCtrlHandler, FALSE);
    g_instance = nullptr;
}

vigine::Result WinOsSignalSource::subscribe(OsSignal signal, IOsSignalListener *listener)
{
    if (!listener)
    {
        return vigine::Result{vigine::Result::Code::Error,
                              "subscribe: null listener"};
    }

    // Hangup (SIGHUP) has no Windows equivalent beyond CTRL_BREAK_EVENT
    // which maps to it best-effort.  User1 and User2 have no mapping at all.
    if (signal == OsSignal::User1 || signal == OsSignal::User2)
    {
        return vigine::Result{vigine::Result::Code::Error,
                              "subscribe: OsSignal::User1/User2 not supported on Windows"};
    }

    {
        std::unique_lock lock(_mutex);
        _listeners.push_back({signal, listener});
    }
    return vigine::Result{vigine::Result::Code::Success};
}

void WinOsSignalSource::unsubscribe(OsSignal signal, IOsSignalListener *listener)
{
    std::unique_lock lock(_mutex);
    _listeners.erase(
        std::remove_if(_listeners.begin(), _listeners.end(),
                       [signal, listener](const Entry &e) {
                           return e.signal == signal && e.listener == listener;
                       }),
        _listeners.end());
}

void WinOsSignalSource::dispatch(OsSignal signal) noexcept
{
    std::vector<Entry> snapshot;
    {
        std::unique_lock lock(_mutex);
        snapshot = _listeners;
    }
    for (auto &entry : snapshot)
    {
        if (entry.signal == signal && entry.listener)
        {
            entry.listener->onOsSignal(signal);
        }
    }
}

} // namespace vigine::eventscheduler

#endif // _WIN32

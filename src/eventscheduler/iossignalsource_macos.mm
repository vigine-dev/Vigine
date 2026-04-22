#if defined(__APPLE__)

#include "iossignalsource_macos.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <vector>

namespace vigine::eventscheduler
{

// Self-pipe trick: the signal handler writes one byte to _pipeFd[1];
// the reader thread reads from _pipeFd[0] and calls listeners.

namespace
{

// Global pointer to the single MacOsSignalSource so the async-signal-safe
// signal handler can reach the pipe write end.
static MacOsSignalSource *g_instance = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Map POSIX signal number to OsSignal.
[[nodiscard]] OsSignal posixToOsSignal(int signum) noexcept
{
    switch (signum)
    {
        case SIGTERM:  return OsSignal::Terminate;
        case SIGINT:   return OsSignal::Interrupt;
        case SIGHUP:   return OsSignal::Hangup;
        case SIGUSR1:  return OsSignal::User1;
        case SIGUSR2:  return OsSignal::User2;
        default:       return OsSignal::Terminate;
    }
}

// Async-signal-safe handler: write signal number to pipe.
void sigHandler(int signum) noexcept
{
    MacOsSignalSource *src = g_instance;
    if (!src)
    {
        return;
    }
    auto byte = static_cast<unsigned char>(signum);
    // write() is async-signal-safe.
    (void)::write(src->pipeWriteFd(), &byte, 1);
}

void installSigaction(int signum)
{
    struct sigaction sa{};
    sa.sa_handler = sigHandler;
    ::sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    ::sigaction(signum, &sa, nullptr);
}

void restoreSigDefault(int signum)
{
    struct sigaction sa{};
    sa.sa_handler = SIG_DFL;
    ::sigemptyset(&sa.sa_mask);
    ::sigaction(signum, &sa, nullptr);
}

} // namespace

MacOsSignalSource::MacOsSignalSource()
    : _stop(false)
{
    // Create self-pipe.
    if (::pipe(_pipeFd) != 0)
    {
        _pipeFd[0] = -1;
        _pipeFd[1] = -1;
        return;
    }

    g_instance = this;

    // Install handlers for all five signals.
    installSigaction(SIGTERM);
    installSigaction(SIGINT);
    installSigaction(SIGHUP);
    installSigaction(SIGUSR1);
    installSigaction(SIGUSR2);

    // Start reader thread.
    _readerThread = std::thread([this] { readerLoop(); });
}

MacOsSignalSource::~MacOsSignalSource()
{
    // Restore default signal handlers.
    restoreSigDefault(SIGTERM);
    restoreSigDefault(SIGINT);
    restoreSigDefault(SIGHUP);
    restoreSigDefault(SIGUSR1);
    restoreSigDefault(SIGUSR2);

    g_instance = nullptr;

    // Signal reader thread to stop.
    _stop.store(true, std::memory_order_release);
    if (_pipeFd[1] >= 0)
    {
        unsigned char sentinel = 0;
        (void)::write(_pipeFd[1], &sentinel, 1);
    }

    if (_readerThread.joinable())
    {
        _readerThread.join();
    }

    if (_pipeFd[0] >= 0) { ::close(_pipeFd[0]); }
    if (_pipeFd[1] >= 0) { ::close(_pipeFd[1]); }
}

vigine::Result MacOsSignalSource::subscribe(OsSignal signal, IOsSignalListener *listener)
{
    if (!listener)
    {
        return vigine::Result{vigine::Result::Code::Error,
                              "subscribe: null listener"};
    }
    {
        std::unique_lock lock(_mutex);
        _listeners.push_back({signal, listener});
    }
    return vigine::Result{vigine::Result::Code::Success};
}

void MacOsSignalSource::unsubscribe(OsSignal signal, IOsSignalListener *listener)
{
    std::unique_lock lock(_mutex);
    _listeners.erase(
        std::remove_if(_listeners.begin(), _listeners.end(),
                       [signal, listener](const Entry &e) {
                           return e.signal == signal && e.listener == listener;
                       }),
        _listeners.end());
}

int MacOsSignalSource::pipeWriteFd() const noexcept
{
    return _pipeFd[1];
}

void MacOsSignalSource::readerLoop()
{
    unsigned char byte = 0;
    while (true)
    {
        ssize_t n = ::read(_pipeFd[0], &byte, 1);
        if (n <= 0)
        {
            // EOF or error — stop.
            break;
        }
        if (_stop.load(std::memory_order_acquire) && byte == 0)
        {
            // Sentinel sent by destructor.
            break;
        }

        OsSignal sig = posixToOsSignal(static_cast<int>(byte));
        std::vector<Entry> snapshot;
        {
            std::unique_lock lock(_mutex);
            snapshot = _listeners;
        }
        for (auto &entry : snapshot)
        {
            if (entry.signal == sig && entry.listener)
            {
                entry.listener->onOsSignal(sig);
            }
        }
    }
}

} // namespace vigine::eventscheduler

#endif // __APPLE__

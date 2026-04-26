#if defined(__linux__)

#include "iossignalsource_posix.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <unistd.h>

namespace vigine::eventscheduler
{

// Global pointer to the single PosixOsSignalSource so the async-signal-safe
// handler can reach the pipe write end. Set once on construction, cleared on
// destruction. The engine instantiates exactly one PosixOsSignalSource.
static PosixOsSignalSource *g_instance = nullptr; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

namespace
{

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

// Async-signal-safe handler: write signal number byte to the self-pipe.
void sigHandler(int signum) noexcept
{
    PosixOsSignalSource *src = g_instance;
    if (!src)
    {
        return;
    }
    auto byte = static_cast<unsigned char>(signum);
    // write() is async-signal-safe per POSIX.1-2017.
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

PosixOsSignalSource::PosixOsSignalSource()
    : _stop(false)
{
    // Create self-pipe with O_CLOEXEC to avoid leaking into child processes.
    if (::pipe2(_pipeFd, O_CLOEXEC) != 0)
    {
        _pipeFd[0] = -1;
        _pipeFd[1] = -1;
        return;
    }

    g_instance = this;

    installSigaction(SIGTERM);
    installSigaction(SIGINT);
    installSigaction(SIGHUP);
    installSigaction(SIGUSR1);
    installSigaction(SIGUSR2);

    _readerThread = std::thread([this] { readerLoop(); });
}

PosixOsSignalSource::~PosixOsSignalSource()
{
    restoreSigDefault(SIGTERM);
    restoreSigDefault(SIGINT);
    restoreSigDefault(SIGHUP);
    restoreSigDefault(SIGUSR1);
    restoreSigDefault(SIGUSR2);

    g_instance = nullptr;

    // Wake reader thread so it can exit cleanly.
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

vigine::Result PosixOsSignalSource::subscribe(OsSignal           signal,
                                              IOsSignalListener *listener)
{
    if (!listener)
    {
        return vigine::Result{vigine::Result::Code::Error, "subscribe: null listener"};
    }
    {
        std::unique_lock lock(_mutex);
        _listeners.push_back({signal, listener});
    }
    return vigine::Result{vigine::Result::Code::Success};
}

void PosixOsSignalSource::unsubscribe(OsSignal signal, IOsSignalListener *listener)
{
    std::unique_lock lock(_mutex);
    _listeners.erase(
        std::remove_if(_listeners.begin(), _listeners.end(),
                       [signal, listener](const Entry &e) {
                           return e.signal == signal && e.listener == listener;
                       }),
        _listeners.end());
}

int PosixOsSignalSource::pipeWriteFd() const noexcept
{
    return _pipeFd[1];
}

void PosixOsSignalSource::readerLoop()
{
    unsigned char byte = 0;
    while (true)
    {
        ssize_t n = ::read(_pipeFd[0], &byte, 1);
        if (n <= 0)
        {
            // EOF or read error — exit the loop.
            break;
        }
        if (_stop.load(std::memory_order_acquire) && byte == 0)
        {
            // Sentinel byte sent by destructor.
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

#endif // __linux__

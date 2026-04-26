#pragma once

#if defined(__linux__)

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include "vigine/api/eventscheduler/iossignalsource.h"
#include "vigine/api/eventscheduler/ossignal.h"
#include "vigine/result.h"

namespace vigine::eventscheduler
{

/**
 * @brief Linux OS-signal source using sigaction + self-pipe trick.
 *
 * An async-signal-safe signal handler writes the raw signal number to a
 * pipe; a dedicated reader thread reads from the pipe and calls listeners
 * on a normal (non-signal) stack, bypassing all async-signal-safety
 * restrictions in listener code.
 *
 * Handles: SIGTERM, SIGINT, SIGHUP, SIGUSR1, SIGUSR2.
 *
 * Thread safety: subscribe/unsubscribe are mutex-protected; the reader
 * thread takes a snapshot of the listener list before dispatching so
 * that mutations during a callback are safe.
 *
 * Private to @c src/platform/linux/. Callers only see @ref IOsSignalSource.
 *
 * INV-10: @c class name does not use I prefix — this is a concrete class.
 */
class PosixOsSignalSource final : public IOsSignalSource
{
  public:
    PosixOsSignalSource();
    ~PosixOsSignalSource() override;

    [[nodiscard]] vigine::Result subscribe(OsSignal           signal,
                                           IOsSignalListener *listener) override;

    void unsubscribe(OsSignal signal, IOsSignalListener *listener) override;

    /**
     * @brief Returns the write end of the self-pipe.
     *
     * Called from the async-signal-safe handler; must not block or allocate.
     */
    [[nodiscard]] int pipeWriteFd() const noexcept;

  private:
    void readerLoop();

    struct Entry
    {
        OsSignal           signal;
        IOsSignalListener *listener{nullptr};
    };

    int                _pipeFd[2]{-1, -1};
    std::mutex         _mutex;
    std::vector<Entry> _listeners;
    std::atomic<bool>  _stop{false};
    std::thread        _readerThread;
};

} // namespace vigine::eventscheduler

#endif // __linux__

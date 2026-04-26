#pragma once

#if defined(__APPLE__)

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
 * @brief macOS OS-signal source using sigaction + self-pipe trick.
 *
 * An async-signal-safe signal handler writes the signal number to a pipe;
 * a dedicated reader thread reads from the pipe and calls listeners.
 * This avoids all async-signal-safety restrictions in the listener code.
 *
 * Private to @c src/eventscheduler/. Callers only see @ref IOsSignalSource.
 */
class MacOsSignalSource final : public IOsSignalSource
{
  public:
    MacOsSignalSource();
    ~MacOsSignalSource() override;

    [[nodiscard]] vigine::Result subscribe(OsSignal signal,
                                           IOsSignalListener *listener) override;

    void unsubscribe(OsSignal signal, IOsSignalListener *listener) override;

    /**
     * @brief Returns the write end of the self-pipe.
     *
     * Called from the async-signal-safe handler; must not block.
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
    std::atomic<bool>  _stop;
    std::thread        _readerThread;
};

} // namespace vigine::eventscheduler

#endif // __APPLE__

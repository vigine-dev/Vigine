#pragma once

#include "vigine/eventscheduler/ossignal.h"
#include "vigine/result.h"

namespace vigine::eventscheduler
{

/**
 * @brief Listener notified when the OS delivers a signal intercepted by
 *        @ref IOsSignalSource.
 *
 * INV-10: @c I prefix for a pure-virtual interface with no state.
 */
class IOsSignalListener
{
  public:
    virtual ~IOsSignalListener() = default;

    /**
     * @brief Called when the OS delivers @p signal.
     *
     * Invoked from the signal handler thread; implementations must be
     * thread-safe and must not block.
     */
    virtual void onOsSignal(OsSignal signal) = 0;

  protected:
    IOsSignalListener() = default;
};

/**
 * @brief Pure-virtual cross-platform OS signal abstraction.
 *
 * @ref IOsSignalSource wraps platform-specific OS signal handling
 * (POSIX sigaction + self-pipe on Linux/macOS; SetConsoleCtrlHandler on
 * Windows) and delivers @ref IOsSignalListener::onOsSignal callbacks on
 * a dedicated reader thread.
 *
 * Platform notes:
 *   - @c OsSignal::Hangup on Windows returns @ref vigine::Result::Code::Error
 *     from @ref subscribe because SIGHUP has no Windows equivalent.
 *   - @c OsSignal::User1 and @c User2 on Windows are unsupported and
 *     return @ref vigine::Result::Code::Error.
 *
 * INV-1: no template parameters.
 * INV-10: @c I prefix for a pure-virtual interface with no state.
 * INV-11: no graph types in this header.
 */
class IOsSignalSource
{
  public:
    virtual ~IOsSignalSource() = default;

    /**
     * @brief Registers @p listener to receive notifications for
     *        @p signal.
     *
     * Returns an error @ref vigine::Result on platforms where @p signal
     * is not supported (for example @c OsSignal::Hangup on Windows).
     * Multiple listeners for the same signal are supported; each
     * receives the callback independently.
     */
    [[nodiscard]] virtual vigine::Result
        subscribe(OsSignal signal, IOsSignalListener *listener) = 0;

    /**
     * @brief Unregisters @p listener from @p signal.
     *
     * Unregistering a listener that is not subscribed is a no-op.
     */
    virtual void unsubscribe(OsSignal signal, IOsSignalListener *listener) = 0;

    IOsSignalSource(const IOsSignalSource &)            = delete;
    IOsSignalSource &operator=(const IOsSignalSource &) = delete;
    IOsSignalSource(IOsSignalSource &&)                 = delete;
    IOsSignalSource &operator=(IOsSignalSource &&)      = delete;

  protected:
    IOsSignalSource() = default;
};

} // namespace vigine::eventscheduler

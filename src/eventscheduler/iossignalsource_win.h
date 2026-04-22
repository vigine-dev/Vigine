#pragma once

#if defined(_WIN32)

#include <mutex>
#include <vector>

#include "vigine/eventscheduler/iossignalsource.h"
#include "vigine/eventscheduler/ossignal.h"
#include "vigine/result.h"

namespace vigine::eventscheduler
{

/**
 * @brief Windows OS-signal source using SetConsoleCtrlHandler.
 *
 * Maps:
 *   - CTRL_C_EVENT     -> OsSignal::Interrupt
 *   - CTRL_CLOSE_EVENT -> OsSignal::Terminate
 *   - CTRL_BREAK_EVENT -> OsSignal::Hangup (best-effort)
 *   - User1 / User2    -> Result::Error (unsupported on Windows)
 *
 * Private to @c src/eventscheduler/. Callers only see @ref IOsSignalSource.
 */
class WinOsSignalSource final : public IOsSignalSource
{
  public:
    WinOsSignalSource();
    ~WinOsSignalSource() override;

    [[nodiscard]] vigine::Result subscribe(OsSignal signal,
                                           IOsSignalListener *listener) override;

    void unsubscribe(OsSignal signal, IOsSignalListener *listener) override;

    /**
     * @brief Called from the Windows console control handler to fan out
     *        to registered listeners.
     *
     * May be invoked from a Windows-created thread; implementations are
     * thread-safe.
     */
    void dispatch(OsSignal signal) noexcept;

  private:
    struct Entry
    {
        OsSignal           signal;
        IOsSignalListener *listener{nullptr};
    };

    std::mutex         _mutex;
    std::vector<Entry> _listeners;
};

} // namespace vigine::eventscheduler

#endif // _WIN32

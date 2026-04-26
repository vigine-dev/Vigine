#pragma once

#include <cstdint>

namespace vigine::eventscheduler
{

/**
 * @brief Closed enumeration of the OS signals the engine can intercept.
 *
 * Exactly five values per plan_16. Each maps to a platform signal:
 *   - @c Terminate  -- SIGTERM on POSIX; CTRL_CLOSE_EVENT on Windows.
 *   - @c Interrupt  -- SIGINT on POSIX; CTRL_C_EVENT on Windows.
 *   - @c Hangup     -- SIGHUP on POSIX; CTRL_BREAK_EVENT on Windows
 *                      (best-effort; returns Result::NotSupported on Windows).
 *   - @c User1      -- SIGUSR1 on POSIX; no native Windows equivalent (no-op).
 *   - @c User2      -- SIGUSR2 on POSIX; no native Windows equivalent (no-op).
 *
 * INV-11: no graph types appear in this header.
 */
enum class OsSignal : std::uint8_t
{
    Terminate = 1,
    Interrupt = 2,
    Hangup    = 3,
    User1     = 4,
    User2     = 5,
};

} // namespace vigine::eventscheduler

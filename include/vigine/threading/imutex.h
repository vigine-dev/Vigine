#pragma once

#include <chrono>

#include "vigine/result.h"

namespace vigine::threading
{
/**
 * @brief Pure-virtual engine-level mutual-exclusion primitive.
 *
 * @ref IMutex is the narrowest lock surface the engine offers. Callers
 * obtain an instance from @ref IThreadManager::createMutex, hand the
 * returned @c std::unique_ptr ownership around, and call @ref lock /
 * @ref tryLock / @ref unlock on it exactly as they would a
 * @c std::mutex. The default factory returns a wrapper around
 * @c std::timed_mutex, so @ref lock with a finite @p timeout forwards to
 * @c std::timed_mutex::try_lock_for. The concrete type stays hidden
 * behind the interface so call sites are insulated from the underlying
 * primitive and from any future swap (for example to a sanitiser-friendly
 * instrumented mutex).
 *
 * Ownership and lifetime: every mutex is owned by the caller via
 * @c std::unique_ptr. The manager that produced the mutex does not
 * retain a back-reference, so destroying the mutex after the manager has
 * been shut down is safe. Destroying a mutex while another thread holds
 * it is a programming error and is not tested for here — callers are
 * expected to release their locks before drop.
 *
 * Thread-safety: every entry point is safe to call from any thread at
 * any time. The locking protocol matches the C++ standard: a thread
 * that holds the lock must release it before another thread can
 * acquire it.
 */
class IMutex
{
  public:
    virtual ~IMutex() = default;

    /**
     * @brief Acquires the mutex, blocking up to @p timeout.
     *
     * Returns a successful @ref Result when the lock was acquired.
     * Returns an error @ref Result when @p timeout elapses before the
     * lock was acquired. The default timeout of
     * @c std::chrono::milliseconds::max asks the implementation to block
     * indefinitely, matching @c std::mutex::lock semantics.
     */
    [[nodiscard]] virtual Result
        lock(std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) = 0;

    /**
     * @brief Attempts to acquire the mutex without blocking.
     *
     * Returns @c true when the lock was acquired on the calling thread
     * and @c false when the mutex is already held by another thread or
     * was already held by the caller. Implementations must match the
     * non-blocking contract — a caller that needs a timed wait should
     * use @ref lock with a non-default @p timeout.
     */
    [[nodiscard]] virtual bool tryLock() = 0;

    /**
     * @brief Releases the mutex.
     *
     * The caller must currently hold the lock. Unlocking a mutex the
     * caller does not hold is undefined per the C++ standard; concrete
     * implementations may or may not detect the misuse.
     */
    virtual void unlock() = 0;

    IMutex(const IMutex &)            = delete;
    IMutex &operator=(const IMutex &) = delete;
    IMutex(IMutex &&)                 = delete;
    IMutex &operator=(IMutex &&)      = delete;

  protected:
    IMutex() = default;
};

} // namespace vigine::threading

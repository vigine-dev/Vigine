#pragma once

#include <chrono>
#include <cstddef>

#include "vigine/result.h"

namespace vigine::threading
{
/**
 * @brief Pure-virtual counting semaphore produced by
 *        @ref IThreadManager::createSemaphore.
 *
 * @ref ISemaphore implements the canonical counting-semaphore protocol:
 * an internal non-negative counter with @ref acquire decrementing (blocks
 * while the counter is zero) and @ref release incrementing. The initial
 * counter value is supplied at construction time through the factory on
 * @ref IThreadManager and cannot be changed afterwards — callers that
 * need a different starting value create a second semaphore.
 *
 * Ownership and lifetime: the semaphore is owned by the caller via
 * @c std::unique_ptr. Destroying the semaphore while another thread
 * blocks on @ref acquire is a programming error.
 *
 * Thread-safety: every entry point is safe to call from any thread.
 * @ref release wakes exactly one pending @ref acquire at a time;
 * implementations are free to pick any waiter (FIFO is not required by
 * this contract).
 */
class ISemaphore
{
  public:
    virtual ~ISemaphore() = default;

    /**
     * @brief Waits until the counter is positive, then decrements it.
     *
     * Blocks up to @p timeout. Returns a successful @ref Result when a
     * permit was acquired. Returns an error @ref Result when @p timeout
     * elapses first. The default timeout of
     * @c std::chrono::milliseconds::max asks the implementation to block
     * indefinitely.
     */
    [[nodiscard]] virtual Result
        acquire(std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) = 0;

    /**
     * @brief Attempts to decrement the counter without blocking.
     *
     * Returns @c true when a permit was acquired and @c false when the
     * counter was already zero. The call never blocks; callers that want
     * a timed wait should use @ref acquire with a non-default @p timeout.
     */
    [[nodiscard]] virtual bool tryAcquire() = 0;

    /**
     * @brief Increments the counter by one and wakes one waiter.
     *
     * The call never blocks. Implementations that bound the counter (to
     * model a binary semaphore or to cap outstanding resources) may
     * saturate at the bound; the default implementation does not cap.
     */
    virtual void release() = 0;

    /**
     * @brief Snapshot of the current counter value.
     *
     * Primarily for diagnostics and smoke tests. Callers must not race
     * on this value — by the time it returns, another thread may have
     * changed it.
     */
    [[nodiscard]] virtual std::size_t count() const = 0;

    ISemaphore(const ISemaphore &)            = delete;
    ISemaphore &operator=(const ISemaphore &) = delete;
    ISemaphore(ISemaphore &&)                 = delete;
    ISemaphore &operator=(ISemaphore &&)      = delete;

  protected:
    ISemaphore() = default;
};

} // namespace vigine::threading

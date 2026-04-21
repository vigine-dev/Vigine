#pragma once

#include <chrono>

#include "vigine/result.h"

namespace vigine::threading
{
/**
 * @brief Caller-held handle to a scheduled @ref IRunnable.
 *
 * Every @ref IThreadManager::schedule / @ref IThreadManager::scheduleOnNamed
 * call returns an @ref ITaskHandle owned by the caller. The handle carries
 * the final @ref Result once the runnable completes and exposes a
 * cooperative cancellation signal.
 *
 * Lifetime: releasing the handle before the runnable completes is legal
 * — the thread manager still runs the work; the final @ref Result is
 * simply discarded. @ref cancel is cooperative: it marks the handle as
 * cancelled and, when possible, removes the runnable from its queue
 * before dispatch. A runnable already in flight is not interrupted
 * asynchronously; the runnable itself decides whether to observe
 * @ref cancellationRequested at a safe point.
 */
class ITaskHandle
{
  public:
    virtual ~ITaskHandle() = default;

    /**
     * @brief Reports whether the runnable has finished (completed,
     *        cancelled, or failed).
     */
    [[nodiscard]] virtual bool ready() const noexcept = 0;

    /**
     * @brief Blocks until the runnable completes and returns its
     *        @ref Result.
     *
     * Returns an error @ref Result if the thread manager is shut down
     * before the runnable had a chance to execute.
     */
    [[nodiscard]] virtual Result wait() = 0;

    /**
     * @brief Blocks until the runnable completes or the timeout elapses.
     *
     * Returns a successful @ref Result when the runnable completes within
     * the timeout; an error @ref Result otherwise. The thread manager
     * does not abandon the runnable on timeout — it keeps executing.
     */
    [[nodiscard]] virtual Result waitFor(std::chrono::milliseconds timeout) = 0;

    /**
     * @brief Cooperative cancellation request.
     *
     * If the runnable has not yet been picked up by a worker, the manager
     * removes it from its queue and the associated @ref wait returns an
     * error @ref Result. If the runnable is already in flight, the
     * request is recorded and the runnable may observe it via
     * @ref cancellationRequested.
     */
    virtual void cancel() noexcept = 0;

    /**
     * @brief Returns @c true when @ref cancel has been called on this
     *        handle.
     */
    [[nodiscard]] virtual bool cancellationRequested() const noexcept = 0;

    ITaskHandle(const ITaskHandle &)            = delete;
    ITaskHandle &operator=(const ITaskHandle &) = delete;
    ITaskHandle(ITaskHandle &&)                 = delete;
    ITaskHandle &operator=(ITaskHandle &&)      = delete;

  protected:
    ITaskHandle() = default;
};

} // namespace vigine::threading

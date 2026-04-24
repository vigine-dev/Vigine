#pragma once

#include <chrono>
#include <cstddef>

#include "vigine/result.h"

namespace vigine::core::threading
{
/**
 * @brief Pure-virtual reusable barrier produced by
 *        @ref IThreadManager::createBarrier.
 *
 * @ref IBarrier implements a standard @c N-party rendezvous. Each party
 * calls @ref arriveAndWait; the call blocks until exactly @c N parties
 * have arrived, then all @c N calls return together and the barrier
 * resets for the next phase. A party that does not want to participate
 * in the next phase calls @ref arriveAndDrop, which counts the current
 * arrival and reduces the party count by one for every subsequent phase.
 *
 * The party count is supplied at construction through the factory on
 * @ref IThreadManager. A barrier with @c N=1 is legal and lets
 * @ref arriveAndWait return immediately without blocking — useful as a
 * no-op in code that only sometimes needs synchronisation.
 *
 * Ownership and lifetime: the barrier is owned by the caller via
 * @c std::unique_ptr. Destroying the barrier while a party is blocked
 * on @ref arriveAndWait is a programming error.
 *
 * Thread-safety: every entry point is safe to call from any thread.
 */
class IBarrier
{
  public:
    virtual ~IBarrier() = default;

    /**
     * @brief Arrives at the barrier and blocks until the last party
     *        arrives, then all blocked parties return together.
     *
     * Blocks up to @p timeout. Returns a successful @ref Result when
     * the phase completed. Returns an error @ref Result when @p timeout
     * elapses before the last party arrived; the barrier is left in a
     * consistent state and the caller is considered to have withdrawn
     * from the current phase.
     */
    [[nodiscard]] virtual Result
        arriveAndWait(std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) = 0;

    /**
     * @brief Arrives at the barrier and withdraws from all future
     *        phases.
     *
     * Counts as one arrival for the current phase without blocking
     * waiters that have not yet arrived. The party count for every
     * subsequent phase decreases by one. Calling @ref arriveAndDrop
     * more times than the construction-time party count is a programming
     * error and leaves the barrier in an undefined state.
     */
    virtual void arriveAndDrop() = 0;

    /**
     * @brief Snapshot of the remaining party count for the current
     *        phase.
     *
     * Primarily for diagnostics and smoke tests. Callers must not race
     * on this value — by the time it returns, another party may have
     * arrived or dropped.
     */
    [[nodiscard]] virtual std::size_t pendingParties() const = 0;

    IBarrier(const IBarrier &)            = delete;
    IBarrier &operator=(const IBarrier &) = delete;
    IBarrier(IBarrier &&)                 = delete;
    IBarrier &operator=(IBarrier &&)      = delete;

  protected:
    IBarrier() = default;
};

} // namespace vigine::core::threading

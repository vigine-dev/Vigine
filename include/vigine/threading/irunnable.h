#pragma once

#include "vigine/result.h"

namespace vigine::threading
{
/**
 * @brief Pure-virtual interface for any unit of work posted to the thread
 *        manager.
 *
 * @ref IRunnable is the only surface the thread manager ever executes
 * directly. Callers wrap domain work in a concrete subclass and hand the
 * instance to @ref IThreadManager::schedule, which takes ownership. No
 * templates appear on the public API: type-erasure is intentionally
 * deferred to the subclass boundary.
 *
 * Subclasses are expected to capture the data they need by value or by
 * owned pointer. The thread manager guarantees that @ref run is invoked
 * at most once per scheduled instance.
 *
 * Exception policy: implementations should not let exceptions escape
 * @ref run. The manager treats an exception as a failure of the
 * dispatcher loop and records a failing @ref Result on the associated
 * task handle; see @ref ITaskHandle::wait.
 */
class IRunnable
{
  public:
    virtual ~IRunnable() = default;

    /**
     * @brief Perform the unit of work.
     *
     * Returns a successful @ref Result on normal completion and an error
     * @ref Result on a business-level failure. The manager propagates
     * the returned value to the associated @ref ITaskHandle.
     */
    [[nodiscard]] virtual Result run() = 0;

    IRunnable(const IRunnable &)            = delete;
    IRunnable &operator=(const IRunnable &) = delete;
    IRunnable(IRunnable &&)                 = delete;
    IRunnable &operator=(IRunnable &&)      = delete;

  protected:
    IRunnable() = default;
};

} // namespace vigine::threading

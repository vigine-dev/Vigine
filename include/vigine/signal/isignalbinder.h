#pragma once

#include "vigine/abstracttask.h"

namespace vigine
{
/**
 * @brief Validates whether two tasks can be connected by a signal.
 *
 * Implementations typically perform interface checks on the emitting and
 * receiving tasks before a signal route is registered in the task flow.
 * This keeps signal wiring explicit and prevents invalid task connections
 * from being accepted during setup.
 *
 * @see ISignal
 * @see ISignalEmiter
 */
class ISignalBinder
{
  public:
    virtual ~ISignalBinder() = default;

    /**
     * @brief Checks whether a signal can connect the given tasks.
     *
     * @param taskEmiter Task that emits the signal.
     * @param taskReceiver Task that should receive the signal.
     *
     * @return `true` when the tasks satisfy the required interfaces and the
     *         signal route may be registered; otherwise `false`.
     */
    [[nodiscard]] virtual bool check(AbstractTask *taskEmiter, AbstractTask *taskReceiver) = 0;
};
} // namespace vigine

#pragma once

#include <chrono>
#include <mutex>

#include "vigine/result.h"
#include "vigine/core/threading/imutex.h"

namespace vigine::core::threading
{
/**
 * @brief Default @ref IMutex implementation over @c std::timed_mutex.
 *
 * The timed variant of the standard mutex is used so that every
 * timeout branch on @ref IMutex::lock maps to a single primitive. For
 * infinite timeouts @ref lock forwards to @c std::timed_mutex::lock;
 * for finite timeouts it forwards to @c try_lock_for. @ref tryLock
 * forwards to @c try_lock. @ref unlock forwards to @c unlock.
 *
 * State is strictly private (INV — strict encapsulation). The class is
 * declared @c final — it is the single engine-shipped concrete mutex,
 * and subclassing would bypass the pure-virtual contract.
 */
class DefaultMutex final : public IMutex
{
  public:
    DefaultMutex() noexcept;
    ~DefaultMutex() override;

    [[nodiscard]] Result
        lock(std::chrono::milliseconds timeout) override;

    [[nodiscard]] bool tryLock() override;

    void unlock() override;

  private:
    std::timed_mutex _mutex;
};

} // namespace vigine::core::threading

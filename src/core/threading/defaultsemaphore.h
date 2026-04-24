#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>

#include "vigine/result.h"
#include "vigine/core/threading/isemaphore.h"

namespace vigine::core::threading
{
/**
 * @brief Default @ref ISemaphore implementation built on
 *        @c std::mutex + @c std::condition_variable.
 *
 * The implementation deliberately avoids @c std::counting_semaphore so
 * that the same code path runs on every supported toolchain. The
 * internal counter is guarded by a @c std::mutex; @ref acquire waits on
 * a single @c std::condition_variable for a positive counter, and
 * @ref release notifies one waiter per increment.
 *
 * State is strictly private (INV — strict encapsulation).
 */
class DefaultSemaphore final : public ISemaphore
{
  public:
    explicit DefaultSemaphore(std::size_t initialCount) noexcept;
    ~DefaultSemaphore() override;

    [[nodiscard]] Result
        acquire(std::chrono::milliseconds timeout) override;

    [[nodiscard]] bool tryAcquire() override;

    void release() override;

    [[nodiscard]] std::size_t count() const override;

  private:
    mutable std::mutex      _mutex;
    std::condition_variable _cv;
    std::size_t             _count;
};

} // namespace vigine::core::threading

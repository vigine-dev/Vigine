#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>

#include "vigine/result.h"
#include "vigine/threading/ibarrier.h"

namespace vigine::threading
{
/**
 * @brief Default @ref IBarrier implementation — a reusable generation-
 *        based barrier on @c std::mutex + @c std::condition_variable.
 *
 * @c std::barrier (C++20) is intentionally avoided to keep the
 * implementation's code path uniform across every supported toolchain
 * and to avoid relying on library features that shipped at different
 * times on different platforms. The generation counter protects
 * against the classic reusable-barrier race — a late arriver from
 * phase @c N cannot block waiters of phase @c N+1 because each
 * @ref arriveAndWait captures the generation it entered on and waits
 * only for that generation to advance.
 *
 * State is strictly private (INV — strict encapsulation).
 */
class DefaultBarrier final : public IBarrier
{
  public:
    explicit DefaultBarrier(std::size_t parties) noexcept;
    ~DefaultBarrier() override;

    [[nodiscard]] Result
        arriveAndWait(std::chrono::milliseconds timeout) override;

    void arriveAndDrop() override;

    [[nodiscard]] std::size_t pendingParties() const override;

  private:
    mutable std::mutex      _mutex;
    std::condition_variable _cv;
    // Party count for every future phase; decreases with arriveAndDrop.
    std::size_t             _parties;
    // Number of arrivals outstanding for the current phase.
    std::size_t             _pending;
    // Generation counter — every time the phase completes it
    // increments, and every waiter compares against the snapshot it
    // captured on entry.
    std::uint64_t           _generation;
};

} // namespace vigine::threading

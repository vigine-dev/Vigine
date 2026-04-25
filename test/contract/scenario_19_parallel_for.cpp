// ---------------------------------------------------------------------------
// Scenario 19 -- vigine::core::threading::parallelFor coverage.
//
// parallelFor is the free helper that fans out a [0, count) range
// across the thread-manager pool and returns one aggregated
// ITaskHandle whose wait() blocks until every chunk has finished.
// The contract is documented in
// include/vigine/core/threading/parallel_for.h:
//
//   - Returns a non-null ITaskHandle (never a raw pointer).
//   - Splits the range into roughly poolSize() chunks; with a
//     degenerate pool (poolSize() == 0) the helper falls back to a
//     single chunk covering the whole range.
//   - wait() blocks until every chunk's body has returned. Once
//     wait() reports success, every index in [0, count) has been
//     visited exactly once.
//   - Body callbacks may run on any pool worker; concurrent
//     invocations are expected for non-trivial counts.
//
// What this scenario pins
// -----------------------
//   1. EveryIndexVisitedOnce -- run a 1024-index range and verify
//      every index is touched exactly once after wait() returns.
//      Catches both "missed an index" (chunk boundary off-by-one)
//      and "visited an index twice" (overlapping ranges) regressions.
//
//   2. WaitBlocksUntilBodiesComplete -- the aggregated handle's
//      wait() must not return until the last chunk's body has
//      returned. We synchronise via an atomic counter the body
//      bumps on entry/exit and assert the in-flight counter is 0
//      after wait() returns successfully.
//
//   3. EmptyRangeReturnsImmediateSuccess -- count == 0 yields a
//      handle that is already settled (ready() reports true and
//      wait() returns Success without blocking).
//
//   4. HandleNonNull -- the helper never hands the caller a null
//      unique_ptr<ITaskHandle>, even on the empty-range edge case.
// ---------------------------------------------------------------------------

#include "fixtures/engine_fixture.h"

#include "vigine/api/context/icontext.h"
#include "vigine/core/threading/itaskhandle.h"
#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/core/threading/parallel_for.h"
#include "vigine/core/threading/threadaffinity.h"
#include "vigine/result.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

namespace vigine::contract
{
namespace
{

using ParallelForCoverage = EngineFixture;

constexpr std::size_t kIndexCount = 1024;

TEST_F(ParallelForCoverage, EveryIndexVisitedOnce)
{
    auto &tm = context().threadManager();

    // Visit counters per index so we can assert each one is touched
    // exactly once. Atomic so the body can increment under concurrent
    // worker invocations without UB.
    std::vector<std::atomic<int>> visits(kIndexCount);
    for (auto &v : visits)
    {
        v.store(0, std::memory_order_relaxed);
    }

    auto handle = vigine::core::threading::parallelFor(
        tm,
        kIndexCount,
        [&visits](std::size_t i) {
            // Bump the per-index counter. Out-of-range indices are a
            // contract violation that the per-index assertion below
            // surfaces.
            if (i < visits.size())
            {
                visits[i].fetch_add(1, std::memory_order_acq_rel);
            }
        });
    ASSERT_NE(handle, nullptr) << "parallelFor must return a non-null handle";

    const vigine::Result waited =
        handle->waitFor(std::chrono::seconds{30});
    ASSERT_TRUE(waited.isSuccess())
        << "parallelFor wait must succeed within 30 s; got: "
        << waited.message();
    EXPECT_TRUE(handle->ready()) << "handle must report ready after wait()";

    // Verify every index visited exactly once. We compute the sum
    // first as a fast smoke (any miss / double-visit shifts the sum
    // away from kIndexCount), then walk the array to attribute
    // failures.
    std::size_t totalVisits = 0;
    std::size_t unique      = 0;
    for (const auto &v : visits)
    {
        const int n = v.load(std::memory_order_acquire);
        totalVisits += static_cast<std::size_t>(n);
        if (n == 1)
        {
            ++unique;
        }
    }
    EXPECT_EQ(totalVisits, kIndexCount)
        << "every index must be visited exactly once; total visits=" << totalVisits;
    EXPECT_EQ(unique, kIndexCount)
        << "every index must have a visit count of 1; unique=" << unique;
}

TEST_F(ParallelForCoverage, WaitBlocksUntilBodiesComplete)
{
    auto &tm = context().threadManager();

    // The body bumps a "started" counter on entry and a "finished"
    // counter on exit. After wait() returns success, both counters
    // must equal kIndexCount: the helper's contract is that wait()
    // does not return until every chunk's body has returned.
    std::atomic<std::size_t> started{0};
    std::atomic<std::size_t> finished{0};

    // Use a small subrange + a per-body sleep so the wait() barrier
    // is genuinely under test. With a tight body that fanned out over
    // 1024 indices the chunks complete before wait() is even called,
    // which makes a broken barrier indistinguishable from a working
    // one (false-pass risk). A 64-index range + 5 ms sleep keeps the
    // body running long enough that the dispatcher must really gate
    // wait() until the last chunk has returned.
    constexpr std::size_t kBarrierIndexCount = 64;

    auto handle = vigine::core::threading::parallelFor(
        tm,
        kBarrierIndexCount,
        [&](std::size_t /*i*/) {
            started.fetch_add(1, std::memory_order_acq_rel);
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
            finished.fetch_add(1, std::memory_order_acq_rel);
        });
    ASSERT_NE(handle, nullptr);

    const vigine::Result waited =
        handle->waitFor(std::chrono::seconds{30});
    ASSERT_TRUE(waited.isSuccess())
        << "parallelFor wait must succeed; got: " << waited.message();

    EXPECT_EQ(started.load(std::memory_order_acquire), kBarrierIndexCount);
    EXPECT_EQ(finished.load(std::memory_order_acquire), kBarrierIndexCount)
        << "wait() must not return before every body has completed";
}

TEST_F(ParallelForCoverage, EmptyRangeReturnsImmediateSuccess)
{
    auto &tm = context().threadManager();

    // Atomic flag so a regression that *does* invoke the body on a
    // pool worker thread cannot race the main thread's read. With a
    // plain bool the assertion would still fire, but the write/read
    // pair would be UB (data race), masking the true failure cause
    // under a sanitiser.
    std::atomic<bool> bodyInvoked{false};
    auto handle = vigine::core::threading::parallelFor(
        tm,
        /*count=*/0,
        [&bodyInvoked](std::size_t) {
            bodyInvoked.store(true, std::memory_order_release);
        });
    ASSERT_NE(handle, nullptr)
        << "parallelFor must return a non-null handle even for an empty range";

    EXPECT_TRUE(handle->ready())
        << "an empty-range handle must already be settled";
    const vigine::Result waited = handle->wait();
    EXPECT_TRUE(waited.isSuccess())
        << "empty-range wait must report Success; got: " << waited.message();
    EXPECT_FALSE(bodyInvoked.load(std::memory_order_acquire))
        << "an empty range must not invoke the body even once";
}

} // namespace
} // namespace vigine::contract

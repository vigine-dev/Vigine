// ---------------------------------------------------------------------------
// Scenario 2 -- threading core + sync primitive round-trip.
//
// Exercises each sync primitive factory (mutex / semaphore / barrier /
// message channel) vended by IThreadManager::create* plus the
// schedule(IRunnable) path. A single thread manager from the shared
// context is enough; the scenario does not need a private stack.
//
// A lambda-wrapping runnable helper keeps the scheduled work small and
// self-contained.
// ---------------------------------------------------------------------------

#include "fixtures/engine_fixture.h"

#include "vigine/api/context/icontext.h"
#include "vigine/result.h"
#include "vigine/core/threading/ibarrier.h"
#include "vigine/core/threading/imessagechannel.h"
#include "vigine/core/threading/imutex.h"
#include "vigine/core/threading/irunnable.h"
#include "vigine/core/threading/isemaphore.h"
#include "vigine/core/threading/itaskhandle.h"
#include "vigine/core/threading/ithreadmanager.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <thread>
#include <utility>

namespace vigine::contract
{
namespace
{

using ThreadingRoundTrip = EngineFixture;

// Minimal IRunnable that flips a flag and returns Success.
class FlagRunnable final : public vigine::core::threading::IRunnable
{
  public:
    explicit FlagRunnable(std::atomic<bool> *flag) noexcept : _flag(flag) {}

    [[nodiscard]] vigine::Result run() override
    {
        _flag->store(true, std::memory_order_release);
        return vigine::Result{};
    }

  private:
    std::atomic<bool> *_flag;
};

TEST_F(ThreadingRoundTrip, ScheduleRunnableFinishesWithSuccess)
{
    auto &tm = context().threadManager();

    std::atomic<bool> flag{false};
    auto handle = tm.schedule(std::make_unique<FlagRunnable>(&flag));
    ASSERT_NE(handle, nullptr);

    const vigine::Result waited = handle->waitFor(std::chrono::seconds{2});
    EXPECT_TRUE(waited.isSuccess())
        << "runnable must finish within 2 s; got: " << waited.message();
    EXPECT_TRUE(flag.load(std::memory_order_acquire));
}

TEST_F(ThreadingRoundTrip, MutexLockUnlockIsOrdered)
{
    auto &tm    = context().threadManager();
    auto  mutex = tm.createMutex();
    ASSERT_NE(mutex, nullptr);

    EXPECT_TRUE(mutex->tryLock())
        << "fresh mutex must be lockable without contention";
    mutex->unlock();

    // Re-lock with the timed entry point and release. The contract
    // stipulates success when the lock is uncontended.
    const vigine::Result locked = mutex->lock(std::chrono::milliseconds{50});
    EXPECT_TRUE(locked.isSuccess())
        << "uncontended timed lock must succeed; got: " << locked.message();
    mutex->unlock();
}

TEST_F(ThreadingRoundTrip, SemaphoreReleaseUnblocksAcquire)
{
    auto &tm        = context().threadManager();
    auto  semaphore = tm.createSemaphore(0);
    ASSERT_NE(semaphore, nullptr);

    // Fire a releaser thread that bumps the counter after 10 ms; the
    // main test thread waits up to 500 ms. Using a condition-variable-
    // free shape here because the test exists to exercise ISemaphore
    // itself, which internally uses a condition variable already.
    std::thread releaser{[&semaphore] {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        semaphore->release();
    }};

    const vigine::Result acquired =
        semaphore->acquire(std::chrono::milliseconds{500});
    releaser.join();

    EXPECT_TRUE(acquired.isSuccess())
        << "semaphore acquire must wake up after release; got: "
        << acquired.message();
}

TEST_F(ThreadingRoundTrip, BarrierArriveAndWaitRendezvous)
{
    auto &tm      = context().threadManager();
    auto  barrier = tm.createBarrier(2);
    ASSERT_NE(barrier, nullptr);

    std::atomic<int> passed{0};
    std::thread party{[&] {
        const vigine::Result r =
            barrier->arriveAndWait(std::chrono::milliseconds{500});
        if (r.isSuccess())
        {
            passed.fetch_add(1, std::memory_order_relaxed);
        }
    }};

    const vigine::Result r =
        barrier->arriveAndWait(std::chrono::milliseconds{500});
    party.join();

    EXPECT_TRUE(r.isSuccess());
    EXPECT_EQ(passed.load(), 1) << "both parties must be released";
}

TEST_F(ThreadingRoundTrip, MessageChannelSendReceive)
{
    auto &tm      = context().threadManager();
    auto  channel = tm.createMessageChannel(4);
    ASSERT_NE(channel, nullptr);
    EXPECT_EQ(channel->capacity(), 4u);

    // Single-value round-trip: payload-less Message carrying only a
    // type id tag. The buffer stays empty on purpose -- the contract
    // tests the ownership transfer, not the encoding path.
    vigine::core::threading::Message outbound{};
    outbound.typeId = vigine::payload::PayloadTypeId{0xBEEF};

    const vigine::Result sent =
        channel->send(std::move(outbound), std::chrono::milliseconds{100});
    EXPECT_TRUE(sent.isSuccess())
        << "empty slot send must succeed; got: " << sent.message();

    vigine::core::threading::Message inbound{};
    const vigine::Result received =
        channel->receive(inbound, std::chrono::milliseconds{100});
    EXPECT_TRUE(received.isSuccess());
    EXPECT_EQ(inbound.typeId.value, 0xBEEFu);

    channel->close();
    EXPECT_TRUE(channel->isClosed());
}

} // namespace
} // namespace vigine::contract

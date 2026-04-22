// ---------------------------------------------------------------------------
// Threading sync-primitive smoke suite.
//
// Exercises the four factory-produced primitives on IThreadManager:
//   * createMutex()          — lock / unlock / try_lock / lockFor timeout.
//   * createSemaphore()      — acquire / release / count / tryAcquire.
//   * createBarrier()        — two-thread arriveAndWait round trip.
//   * createMessageChannel() — send / receive / close drain semantics.
//
// The threading contract layer is Level-0 of the engine; every
// wrapper above depends on these primitives working correctly. A
// smoke target here turns a regression in the primitives into a
// localised failure instead of a mysterious crash three layers up.
// ---------------------------------------------------------------------------

#include "vigine/result.h"
#include "vigine/threading/factory.h"
#include "vigine/threading/ibarrier.h"
#include "vigine/threading/imessagechannel.h"
#include "vigine/threading/imutex.h"
#include "vigine/threading/isemaphore.h"
#include "vigine/threading/ithreadmanager.h"
#include "vigine/threading/threadmanagerconfig.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

using vigine::Result;
using vigine::threading::createThreadManager;
using vigine::threading::IBarrier;
using vigine::threading::IMessageChannel;
using vigine::threading::IMutex;
using vigine::threading::ISemaphore;
using vigine::threading::IThreadManager;
using vigine::threading::Message;
using vigine::threading::ThreadManagerConfig;

namespace
{

[[nodiscard]] std::unique_ptr<IThreadManager> smallThreadManager()
{
    ThreadManagerConfig cfg{};
    cfg.poolSize = 2;
    return createThreadManager(cfg);
}

} // namespace

// ---------------------------------------------------------------------------
// IMutex
// ---------------------------------------------------------------------------

TEST(SyncPrimitivesSmoke, MutexLockUnlockRoundTrip)
{
    auto tm = smallThreadManager();
    ASSERT_NE(tm, nullptr);

    auto mtx = tm->createMutex();
    ASSERT_NE(mtx, nullptr);

    // Uncontended lock succeeds immediately.
    const Result r = mtx->lock();
    EXPECT_TRUE(r.isSuccess());

    mtx->unlock();

    // Re-lock after unlock succeeds too — the wrapper does not cache
    // any lock-held state on the caller side.
    EXPECT_TRUE(mtx->lock().isSuccess());
    mtx->unlock();
}

TEST(SyncPrimitivesSmoke, MutexTryLockReportsContention)
{
    auto tm  = smallThreadManager();
    auto mtx = tm->createMutex();

    ASSERT_TRUE(mtx->lock().isSuccess());

    // Sibling thread attempts try_lock — the current thread holds the
    // lock, so try_lock must fail.
    std::atomic<bool> contention{false};
    std::thread       worker{[&] { contention = mtx->tryLock(); }};
    worker.join();
    EXPECT_FALSE(contention.load());

    mtx->unlock();

    // Same thread tries again; now uncontended. Even from a fresh
    // worker.
    std::atomic<bool> second{false};
    std::thread       worker2{[&] {
        if (mtx->tryLock())
        {
            mtx->unlock();
            second = true;
        }
    }};
    worker2.join();
    EXPECT_TRUE(second.load());
}

TEST(SyncPrimitivesSmoke, MutexLockForTimeout)
{
    auto tm  = smallThreadManager();
    auto mtx = tm->createMutex();

    ASSERT_TRUE(mtx->lock().isSuccess());

    // Sibling thread tries to lock with a bounded timeout; the current
    // thread holds the lock, so the call must return an error Result
    // after the timeout elapses — not block indefinitely.
    std::atomic<bool> timedOut{false};
    std::thread       worker{[&] {
        const auto start = std::chrono::steady_clock::now();
        const Result r   = mtx->lock(std::chrono::milliseconds{25});
        const auto   dt  = std::chrono::steady_clock::now() - start;
        if (r.isError() && dt >= std::chrono::milliseconds{20})
        {
            timedOut = true;
        }
    }};
    worker.join();
    EXPECT_TRUE(timedOut.load());

    mtx->unlock();
}

// ---------------------------------------------------------------------------
// ISemaphore
// ---------------------------------------------------------------------------

TEST(SyncPrimitivesSmoke, SemaphoreAcquireReleaseRoundTrip)
{
    auto tm  = smallThreadManager();
    auto sem = tm->createSemaphore(/*initialCount=*/2);
    ASSERT_NE(sem, nullptr);

    EXPECT_EQ(sem->count(), 2u);

    EXPECT_TRUE(sem->acquire().isSuccess());
    EXPECT_EQ(sem->count(), 1u);

    EXPECT_TRUE(sem->acquire().isSuccess());
    EXPECT_EQ(sem->count(), 0u);

    EXPECT_FALSE(sem->tryAcquire());

    sem->release();
    EXPECT_EQ(sem->count(), 1u);

    EXPECT_TRUE(sem->tryAcquire());
    EXPECT_EQ(sem->count(), 0u);
}

// ---------------------------------------------------------------------------
// IBarrier — two-party round trip.
// ---------------------------------------------------------------------------

TEST(SyncPrimitivesSmoke, BarrierTwoPartyArriveAndWait)
{
    auto tm  = smallThreadManager();
    auto bar = tm->createBarrier(/*parties=*/2);
    ASSERT_NE(bar, nullptr);

    std::atomic<int> reached{0};
    std::thread      w1{[&] {
        EXPECT_TRUE(bar->arriveAndWait().isSuccess());
        reached.fetch_add(1);
    }};
    std::thread w2{[&] {
        EXPECT_TRUE(bar->arriveAndWait().isSuccess());
        reached.fetch_add(1);
    }};

    w1.join();
    w2.join();
    EXPECT_EQ(reached.load(), 2);
}

// ---------------------------------------------------------------------------
// IMessageChannel — send / receive / close.
// ---------------------------------------------------------------------------

TEST(SyncPrimitivesSmoke, MessageChannelSendReceiveRoundTrip)
{
    auto tm = smallThreadManager();
    auto ch = tm->createMessageChannel(/*capacity=*/4);
    ASSERT_NE(ch, nullptr);

    Message sent;
    sent.typeId    = vigine::payload::PayloadTypeId{0x12345u};
    sent.bytes     = std::make_unique<std::byte[]>(3);
    sent.bytes[0]  = std::byte{1};
    sent.bytes[1]  = std::byte{2};
    sent.bytes[2]  = std::byte{3};
    sent.sizeBytes = 3;

    EXPECT_TRUE(ch->send(std::move(sent)).isSuccess());

    Message received;
    EXPECT_TRUE(ch->receive(received).isSuccess());
    EXPECT_EQ(received.typeId.value, 0x12345u);
    ASSERT_EQ(received.sizeBytes, 3u);
    EXPECT_EQ(received.bytes[0], std::byte{1});
    EXPECT_EQ(received.bytes[1], std::byte{2});
    EXPECT_EQ(received.bytes[2], std::byte{3});
}

TEST(SyncPrimitivesSmoke, MessageChannelCloseRejectsFurtherSends)
{
    auto tm = smallThreadManager();
    auto ch = tm->createMessageChannel(/*capacity=*/4);

    ch->close();
    EXPECT_TRUE(ch->isClosed());

    Message attempt;
    EXPECT_FALSE(ch->send(std::move(attempt)).isSuccess());
}

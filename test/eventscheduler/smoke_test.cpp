#include "vigine/eventscheduler/defaulteventscheduler.h"
#include "vigine/eventscheduler/eventconfig.h"
#include "vigine/eventscheduler/ieventhandle.h"
#include "vigine/eventscheduler/ieventscheduler.h"
#include "vigine/eventscheduler/iossignalsource.h"
#include "vigine/eventscheduler/itimersource.h"
#include "vigine/eventscheduler/ossignal.h"
#include "vigine/messaging/abstractmessagetarget.h"
#include "vigine/messaging/iconnectiontoken.h"
#include "vigine/messaging/imessage.h"
#include "vigine/messaging/messagekind.h"
#include "vigine/messaging/targetkind.h"
#include "vigine/result.h"
#include "vigine/threading/factory.h"
#include "vigine/threading/ithreadmanager.h"
#include "vigine/threading/threadmanagerconfig.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

// ---------------------------------------------------------------------------
// Test doubles
// ---------------------------------------------------------------------------

namespace
{

using namespace vigine::eventscheduler;

// Minimal ITimerSource that fires manually via triggerTimer().
class MockTimerSource final : public ITimerSource
{
  public:
    struct Entry
    {
        std::uint64_t       id{0};
        ITimerFiredListener *listener{nullptr};
        bool                active{true};
    };

    [[nodiscard]] std::uint64_t
        armOneShot(std::chrono::milliseconds /*delay*/,
                   ITimerFiredListener       *listener) override
    {
        std::uint64_t id = _nextId++;
        std::unique_lock lock(_mutex);
        _entries.push_back({id, listener, true});
        return id;
    }

    [[nodiscard]] std::uint64_t
        armPeriodic(std::chrono::milliseconds /*period*/,
                    std::size_t               /*count*/,
                    ITimerFiredListener       *listener) override
    {
        return armOneShot(std::chrono::milliseconds{0}, listener);
    }

    void disarm(std::uint64_t timerId) override
    {
        std::unique_lock lock(_mutex);
        for (auto &e : _entries)
        {
            if (e.id == timerId)
            {
                e.active = false;
            }
        }
    }

    void triggerAll()
    {
        std::vector<Entry> snapshot;
        {
            std::unique_lock lock(_mutex);
            snapshot = _entries;
        }
        for (auto &e : snapshot)
        {
            if (e.active && e.listener)
            {
                e.listener->onTimerFired(e.id);
            }
        }
    }

  private:
    std::uint64_t      _nextId{1};
    std::mutex         _mutex;
    std::vector<Entry> _entries;
};

// Minimal IOsSignalSource that fires manually via fireSignal().
class MockOsSignalSource final : public IOsSignalSource
{
  public:
    struct Entry
    {
        OsSignal           signal;
        IOsSignalListener *listener{nullptr};
    };

    [[nodiscard]] vigine::Result subscribe(OsSignal signal,
                                           IOsSignalListener *listener) override
    {
        if (!listener)
        {
            return vigine::Result{vigine::Result::Code::Error, "null listener"};
        }
        std::unique_lock lock(_mutex);
        _entries.push_back({signal, listener});
        return vigine::Result{vigine::Result::Code::Success};
    }

    void unsubscribe(OsSignal signal, IOsSignalListener *listener) override
    {
        std::unique_lock lock(_mutex);
        _entries.erase(
            std::remove_if(_entries.begin(), _entries.end(),
                           [signal, listener](const Entry &e) {
                               return e.signal == signal && e.listener == listener;
                           }),
            _entries.end());
    }

    void fireSignal(OsSignal signal)
    {
        std::vector<Entry> snapshot;
        {
            std::unique_lock lock(_mutex);
            snapshot = _entries;
        }
        for (auto &e : snapshot)
        {
            if (e.signal == signal && e.listener)
            {
                e.listener->onOsSignal(signal);
            }
        }
    }

  private:
    std::mutex         _mutex;
    std::vector<Entry> _entries;
};

// Concrete message target that counts delivered messages.
class CountingTarget final : public vigine::messaging::AbstractMessageTarget
{
  public:
    [[nodiscard]] vigine::messaging::TargetKind targetKind() const noexcept override
    {
        return vigine::messaging::TargetKind::User;
    }

    void onMessage(const vigine::messaging::IMessage &msg) override
    {
        EXPECT_EQ(msg.kind(), vigine::messaging::MessageKind::Event);
        _count.fetch_add(1, std::memory_order_relaxed);
    }

    [[nodiscard]] int count() const noexcept
    {
        return _count.load(std::memory_order_relaxed);
    }

  private:
    std::atomic<int> _count{0};
};

// Helper: create a thread manager for tests.
std::unique_ptr<vigine::threading::IThreadManager> makeThreadManager()
{
    vigine::threading::ThreadManagerConfig cfg;
    cfg.poolSize = 1;
    return vigine::threading::createThreadManager(cfg);
}

// ---------------------------------------------------------------------------
// Scenario 1: one-shot timer fires once and delivers to target.
// ---------------------------------------------------------------------------

TEST(EventSchedulerSmoke, OneShotTimerFiresOnce)
{
    auto threadMgr = makeThreadManager();
    MockTimerSource   timerSrc;
    MockOsSignalSource osSrc;

    auto scheduler = createEventScheduler(*threadMgr, timerSrc, osSrc);
    ASSERT_NE(scheduler, nullptr);

    CountingTarget target;

    EventConfig cfg;
    cfg.delay      = std::chrono::milliseconds{10};
    cfg.period     = std::chrono::milliseconds{0};
    cfg.useOsSignal = false;

    auto handle = scheduler->schedule(cfg, &target);
    ASSERT_NE(handle, nullptr);
    EXPECT_TRUE(handle->active());

    // Simulate timer firing.
    timerSrc.triggerAll();

    EXPECT_EQ(target.count(), 1);

    // After one-shot fires, handle should be inactive.
    EXPECT_FALSE(handle->active());
}

// ---------------------------------------------------------------------------
// Scenario 2: cancel prevents delivery.
// ---------------------------------------------------------------------------

TEST(EventSchedulerSmoke, CancelPreventsDelivery)
{
    auto threadMgr = makeThreadManager();
    MockTimerSource   timerSrc;
    MockOsSignalSource osSrc;

    auto scheduler = createEventScheduler(*threadMgr, timerSrc, osSrc);

    CountingTarget target;

    EventConfig cfg;
    cfg.delay       = std::chrono::milliseconds{10};
    cfg.period      = std::chrono::milliseconds{0};
    cfg.useOsSignal = false;

    auto handle = scheduler->schedule(cfg, &target);
    ASSERT_NE(handle, nullptr);

    // Cancel before the timer fires.
    handle->cancel();
    EXPECT_FALSE(handle->active());

    timerSrc.triggerAll();

    // Target should have received nothing.
    EXPECT_EQ(target.count(), 0);
}

// ---------------------------------------------------------------------------
// Scenario 3: OS signal triggers delivery.
// ---------------------------------------------------------------------------

TEST(EventSchedulerSmoke, OsSignalTriggersDelivery)
{
    auto threadMgr = makeThreadManager();
    MockTimerSource   timerSrc;
    MockOsSignalSource osSrc;

    auto scheduler = createEventScheduler(*threadMgr, timerSrc, osSrc);

    CountingTarget target;

    EventConfig cfg;
    cfg.useOsSignal = true;
    cfg.osSignal    = OsSignal::Interrupt;

    auto handle = scheduler->schedule(cfg, &target);
    ASSERT_NE(handle, nullptr);
    EXPECT_TRUE(handle->active());

    // Fire SIGINT.
    osSrc.fireSignal(OsSignal::Interrupt);

    EXPECT_EQ(target.count(), 1);
}

} // namespace

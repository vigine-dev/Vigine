// ---------------------------------------------------------------------------
// Scenario 4 -- subscription facade (ISignalEmitter) round-trip.
//
// The signal facade hides MessageKind::Signal / RouteMode::FanOut behind
// emit() + subscribeSignal() entry points. The scenario exercises three
// observable properties of the facade:
//
//   1. emit() returns a success Result for a non-null payload.
//   2. emit() returns an error Result for a null payload.
//   3. When emit() is invoked from a worker thread (scheduled through
//      the injected IThreadManager), the subscriber's onMessage lands on
//      that worker thread, not on the test thread. This asserts that the
//      signal-emitter facade preserves caller-thread dispatch end-to-end
//      across the IThreadManager::schedule -> emit -> bus -> subscriber
//      path -- the substrate needed by the async affinity branch in
//      TaskFlow::signal. The inline-default cases above stay byte-
//      identical so regressions on the default path remain visible.
// ---------------------------------------------------------------------------

#include "fixtures/engine_fixture.h"

#include "vigine/api/context/icontext.h"
#include "vigine/api/messaging/imessage.h"
#include "vigine/api/messaging/isubscriber.h"
#include "vigine/api/messaging/isubscriptiontoken.h"
#include "vigine/api/messaging/messagefilter.h"
#include "vigine/api/messaging/messagekind.h"
#include "vigine/api/messaging/payload/payloadtypeid.h"
#include "vigine/result.h"
#include "vigine/impl/messaging/signalemitter.h"
#include "vigine/api/messaging/isignalemitter.h"
#include "vigine/api/messaging/payload/isignalpayload.h"
#include "vigine/core/threading/irunnable.h"
#include "vigine/core/threading/itaskhandle.h"
#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/core/threading/threadaffinity.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace vigine::contract
{
namespace
{

class SignalPayload final : public vigine::messaging::ISignalPayload
{
  public:
    explicit SignalPayload(vigine::payload::PayloadTypeId id) noexcept : _id(id) {}

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        return _id;
    }

    [[nodiscard]] std::unique_ptr<vigine::messaging::ISignalPayload>
        clone() const override
    {
        return std::make_unique<SignalPayload>(_id);
    }

  private:
    vigine::payload::PayloadTypeId _id;
};

/**
 * @brief Subscriber that records the dispatch thread id and bumps an
 *        atomic hit counter on each onMessage callback.
 *
 * The thread id is stored under a mutex so the test thread sees a stable
 * value once @c hits() returns a non-zero count.
 */
class ThreadRecordingSubscriber final : public vigine::messaging::ISubscriber
{
  public:
    [[nodiscard]] vigine::messaging::DispatchResult
        onMessage(const vigine::messaging::IMessage & /*message*/) override
    {
        {
            std::lock_guard<std::mutex> lock{_m};
            _observed = std::this_thread::get_id();
        }
        _hits.fetch_add(1, std::memory_order_acq_rel);
        return vigine::messaging::DispatchResult::Handled;
    }

    [[nodiscard]] std::uint32_t hits() const noexcept
    {
        return _hits.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::thread::id observed() const
    {
        std::lock_guard<std::mutex> lock{_m};
        return _observed;
    }

  private:
    mutable std::mutex         _m;
    std::thread::id            _observed{};
    std::atomic<std::uint32_t> _hits{0};
};

/**
 * @brief Runnable that calls @ref vigine::messaging::ISignalEmitter::emit
 *        with a fresh @ref SignalPayload carrying @p typeId.
 *
 * Used to push an emit() onto the thread manager's pool so the dispatch
 * thread differs from the test main thread. The emitter pointer is a
 * non-owning raw pointer kept stable by the test fixture's stack-local
 * unique_ptr.
 */
class EmitRunnable final : public vigine::core::threading::IRunnable
{
  public:
    EmitRunnable(vigine::messaging::ISignalEmitter *emitter,
                 vigine::payload::PayloadTypeId         typeId) noexcept
        : _emitter(emitter)
        , _typeId(typeId)
    {
    }

    [[nodiscard]] vigine::Result run() override
    {
        if (_emitter == nullptr)
        {
            return vigine::Result{vigine::Result::Code::Error,
                                  "EmitRunnable: null emitter"};
        }
        return _emitter->emit(std::make_unique<SignalPayload>(_typeId));
    }

  private:
    vigine::messaging::ISignalEmitter *_emitter;
    vigine::payload::PayloadTypeId         _typeId;
};

using SignalEmitter = EngineFixture;

TEST_F(SignalEmitter, EmitReturnsSuccessForValidPayload)
{
    auto emitter =
        vigine::messaging::createSignalEmitter(context().threadManager());
    ASSERT_NE(emitter, nullptr);

    auto payload =
        std::make_unique<SignalPayload>(vigine::payload::PayloadTypeId{0x20101u});

    const vigine::Result r = emitter->emit(std::move(payload));
    EXPECT_TRUE(r.isSuccess())
        << "emit with a valid payload must succeed; got: " << r.message();
}

TEST_F(SignalEmitter, EmitRejectsNullPayload)
{
    auto emitter =
        vigine::messaging::createSignalEmitter(context().threadManager());
    ASSERT_NE(emitter, nullptr);

    const vigine::Result r = emitter->emit(nullptr);
    EXPECT_TRUE(r.isError())
        << "emit with a null payload must report an error";
}

TEST_F(SignalEmitter, AsyncDeliveryCrossesThreadBoundaries)
{
    // A user-range PayloadTypeId past the example-window reservation
    // (0x20101 MouseButtonDown, 0x20102 KeyDown). Keeps the test
    // self-contained and avoids colliding with any engine-owned id.
    constexpr vigine::payload::PayloadTypeId kTestPayloadTypeId{0x20201u};

    // Shared-pool bus exercises the facade overload plumbed through
    // createSignalEmitter(tm, sharedBusConfig()). A subscriber registered
    // via subscribeSignal() is the surface TaskFlow::signal relies on.
    auto emitter = vigine::messaging::createSignalEmitter(
        context().threadManager(),
        vigine::messaging::sharedBusConfig());
    ASSERT_NE(emitter, nullptr);

    ThreadRecordingSubscriber subscriber{};

    vigine::messaging::MessageFilter filter{};
    filter.kind   = vigine::messaging::MessageKind::Signal;
    filter.typeId = kTestPayloadTypeId;

    auto token = emitter->subscribeSignal(filter, &subscriber);
    ASSERT_NE(token, nullptr);
    EXPECT_TRUE(token->active());

    const std::thread::id testThreadId = std::this_thread::get_id();

    // Schedule emit() on a pool worker. This is the ThreadAffinity::Pool
    // branch TaskFlow::signal uses for non-Any affinities: the caller
    // hands the emit to an IThreadManager worker, the worker invokes
    // emit(), and the bus dispatches to the subscriber on that same
    // worker (Shared policy drains on the post-caller thread today).
    // What the assertion really covers: the facade preserves the caller
    // thread throughout the dispatch, and that caller can be a worker.
    auto handle = context().threadManager().schedule(
        std::make_unique<EmitRunnable>(emitter.get(), kTestPayloadTypeId),
        vigine::core::threading::ThreadAffinity::Pool);
    ASSERT_NE(handle, nullptr);

    // Bounded wait on the scheduled emit. 1 s is generous; the pool
    // handoff normally completes within a few ms even on a loaded CI.
    const vigine::Result scheduled =
        handle->waitFor(std::chrono::milliseconds{1000});
    ASSERT_TRUE(scheduled.isSuccess())
        << "scheduled emit must complete within 1s; got: "
        << scheduled.message();

    // The runnable reported success, which means emit() returned success
    // too. Dispatch is synchronous inside that emit() call, so hits() is
    // already non-zero by the time the handle reports ready. Poll a few
    // times just to keep the assertion insensitive to any future move
    // of dispatch off the emit() call site.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{1};
    while (subscriber.hits() == 0u && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }

    ASSERT_GE(subscriber.hits(), 1u)
        << "subscriber must receive at least one dispatch after the "
           "pool-scheduled emit completed; hits == 0 means the "
           "subscription-dispatch path is broken";
    EXPECT_NE(subscriber.observed(), testThreadId)
        << "dispatch landed on the test thread, which means the emit() "
           "did not actually run on a pool worker; the async path "
           "collapsed back to the test thread";
    EXPECT_NE(subscriber.observed(), std::thread::id{})
        << "observed thread id must be a real worker id, not a default-"
           "constructed sentinel";
}

} // namespace
} // namespace vigine::contract

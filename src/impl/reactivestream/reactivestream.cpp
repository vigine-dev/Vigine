#include "vigine/impl/reactivestream/reactivestream.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "vigine/api/reactivestream/factory.h"
#include "vigine/api/reactivestream/ireactivesubscriber.h"
#include "vigine/api/reactivestream/ireactivesubscription.h"
#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/api/messaging/imessagebus.h"
#include "vigine/api/messaging/imessagepayload.h"
#include "vigine/api/messaging/isubscriber.h"
#include "vigine/api/messaging/isubscriptiontoken.h"
#include "vigine/api/messaging/messagefilter.h"
#include "vigine/api/messaging/messagekind.h"
#include "vigine/result.h"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace vigine::reactivestream
{

namespace
{

// ---------------------------------------------------------------------------
// SubscriptionState — shared between the subscriber-side token and the
// stream's delivery path.
// ---------------------------------------------------------------------------

struct SubscriptionState
{
    IReactiveSubscriber         *subscriber{nullptr};
    std::atomic<std::size_t>     demand{0};
    std::atomic<bool>            cancelled{false};
    std::atomic<bool>            terminal{false};

    explicit SubscriptionState(IReactiveSubscriber *sub) : subscriber(sub) {}
};

// ---------------------------------------------------------------------------
// SubscriberToken — RAII handle given to the subscriber via onSubscribe().
// Controls demand and cancellation from the subscriber's side.
// ---------------------------------------------------------------------------

class SubscriberToken final : public IReactiveSubscription
{
  public:
    SubscriberToken(std::shared_ptr<SubscriptionState>  state,
                    std::uint64_t                        id,
                    std::function<void(std::uint64_t)>   removeCallback)
        : _state(std::move(state))
        , _id(id)
        , _removeCallback(std::move(removeCallback))
    {
    }

    ~SubscriberToken() override
    {
        cancel();
    }

    void request(std::size_t n) noexcept override
    {
        if (n == 0)
        {
            return;
        }

        if (_state->cancelled.load(std::memory_order_acquire) ||
            _state->terminal.load(std::memory_order_acquire))
        {
            return;
        }

        std::size_t current = _state->demand.load(std::memory_order_relaxed);
        std::size_t desired;
        constexpr std::size_t kMax = std::numeric_limits<std::size_t>::max();
        do
        {
            desired = (current >= kMax - n) ? kMax : current + n;
        }
        while (!_state->demand.compare_exchange_weak(
            current, desired,
            std::memory_order_release,
            std::memory_order_relaxed));
    }

    void cancel() noexcept override
    {
        bool wasCancelled = _state->cancelled.exchange(true, std::memory_order_release);
        if (!wasCancelled)
        {
            _removeCallback(_id);
        }
    }

    SubscriberToken(const SubscriberToken &)            = delete;
    SubscriberToken &operator=(const SubscriberToken &) = delete;
    SubscriberToken(SubscriberToken &&)                 = delete;
    SubscriberToken &operator=(SubscriberToken &&)      = delete;

  private:
    std::shared_ptr<SubscriptionState>  _state;
    std::uint64_t                       _id;
    std::function<void(std::uint64_t)>  _removeCallback;
};

// ---------------------------------------------------------------------------
// StreamSubscriptionHandle — returned to the caller of subscribe().
// This is a separate RAII object from the subscriber's token; it represents
// the engine-side view of the subscription slot.
// Dropping or cancelling this handle removes the slot from the registry
// without signalling the subscriber (the subscriber's own token is the
// demand-side handle).
// ---------------------------------------------------------------------------

class StreamSubscriptionHandle final : public IReactiveSubscription
{
  public:
    StreamSubscriptionHandle(std::uint64_t                       id,
                             std::function<void(std::uint64_t)>  removeCallback)
        : _id(id)
        , _removeCallback(std::move(removeCallback))
    {
    }

    ~StreamSubscriptionHandle() override = default;

    void request(std::size_t /*n*/) noexcept override
    {
        // No-op — demand is controlled by the subscriber's SubscriberToken.
    }

    void cancel() noexcept override
    {
        _removeCallback(_id);
    }

    StreamSubscriptionHandle(const StreamSubscriptionHandle &)            = delete;
    StreamSubscriptionHandle &operator=(const StreamSubscriptionHandle &) = delete;
    StreamSubscriptionHandle(StreamSubscriptionHandle &&)                 = delete;
    StreamSubscriptionHandle &operator=(StreamSubscriptionHandle &&)      = delete;

  private:
    std::uint64_t                       _id;
    std::function<void(std::uint64_t)>  _removeCallback;
};

// ---------------------------------------------------------------------------
// BusSubscriber — listens on ReactiveSignal messages on the bus.
// Actual item delivery goes through ReactiveStream::publish().
// ---------------------------------------------------------------------------

class BusSubscriber final : public vigine::messaging::ISubscriber
{
  public:
    BusSubscriber() = default;

    [[nodiscard]] vigine::messaging::DispatchResult
        onMessage(const vigine::messaging::IMessage & /*msg*/) override
    {
        // ReactiveSignal received from bus. The reactive stream facade wires
        // to the bus for protocol compliance; direct item delivery is driven
        // by ReactiveStream::publish() rather than payload cloning.
        return vigine::messaging::DispatchResult::Handled;
    }
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct ReactiveStream::Impl
{
    vigine::messaging::IMessageBus    &bus;
    vigine::core::threading::IThreadManager &threadManager;

    std::mutex                                                                registryMutex;
    std::unordered_map<std::uint64_t, std::shared_ptr<SubscriptionState>>    subscriptions;
    std::uint64_t                                                             nextId{1};

    std::unique_ptr<vigine::messaging::ISubscriptionToken>  busToken;
    BusSubscriber                                           busSubscriber;

    std::atomic<bool> shutdownFlag{false};

    explicit Impl(vigine::messaging::IMessageBus    &bus_,
                  vigine::core::threading::IThreadManager &tm_)
        : bus(bus_)
        , threadManager(tm_)
    {
        vigine::messaging::MessageFilter filter;
        filter.kind = vigine::messaging::MessageKind::ReactiveSignal;
        busToken    = bus.subscribe(filter, &busSubscriber);
    }

    std::vector<std::shared_ptr<SubscriptionState>> snapshot()
    {
        std::lock_guard<std::mutex> lk(registryMutex);
        std::vector<std::shared_ptr<SubscriptionState>> result;
        result.reserve(subscriptions.size());
        for (auto &[id, state] : subscriptions)
        {
            result.push_back(state);
        }
        return result;
    }

    void removeEntry(std::uint64_t id)
    {
        std::lock_guard<std::mutex> lk(registryMutex);
        subscriptions.erase(id);
    }
};

// ---------------------------------------------------------------------------
// ReactiveStream
// ---------------------------------------------------------------------------

ReactiveStream::ReactiveStream(vigine::messaging::IMessageBus    &bus,
                                             vigine::core::threading::IThreadManager &threadManager)
    : AbstractReactiveStream(bus)
    , _impl(std::make_unique<Impl>(bus, threadManager))
{
}

ReactiveStream::~ReactiveStream()
{
    shutdown();
}

std::unique_ptr<IReactiveSubscription>
ReactiveStream::subscribe(IReactiveSubscriber *subscriber)
{
    if (!subscriber)
    {
        return nullptr;
    }

    if (_impl->shutdownFlag.load(std::memory_order_acquire))
    {
        return nullptr;
    }

    auto state = std::make_shared<SubscriptionState>(subscriber);

    std::uint64_t id;
    {
        std::lock_guard<std::mutex> lk(_impl->registryMutex);
        id = _impl->nextId++;
        _impl->subscriptions.emplace(id, state);
    }

    // Subscriber-side token — handed to the subscriber via onSubscribe.
    auto subscriberToken = std::make_unique<SubscriberToken>(
        state, id,
        [this](std::uint64_t eid) { _impl->removeEntry(eid); });

    // Engine-side handle — returned to the caller.
    auto handle = std::make_unique<StreamSubscriptionHandle>(
        id,
        [this](std::uint64_t eid) { _impl->removeEntry(eid); });

    // Call onSubscribe — subscriber receives its RAII demand-control token.
    subscriber->onSubscribe(std::move(subscriberToken));

    return handle;
}

vigine::Result ReactiveStream::publish(
    std::unique_ptr<vigine::messaging::IMessagePayload> payload)
{
    if (!payload)
    {
        return vigine::Result{vigine::Result::Code::Error, "null payload"};
    }

    if (_impl->shutdownFlag.load(std::memory_order_acquire))
    {
        return vigine::Result{vigine::Result::Code::Error, "stream shut down"};
    }

    auto snapshot = _impl->snapshot();

    // Deliver to the first active subscriber with non-zero demand.
    // (Cold publisher: once delivered, ownership is consumed.)
    for (auto &state : snapshot)
    {
        if (state->cancelled.load(std::memory_order_acquire) ||
            state->terminal.load(std::memory_order_acquire))
        {
            continue;
        }

        constexpr std::size_t kUnbounded = std::numeric_limits<std::size_t>::max();
        std::size_t current = state->demand.load(std::memory_order_acquire);
        bool claimed = false;
        while (current > 0)
        {
            if (current == kUnbounded)
            {
                // Unbounded demand — no decrement, just claim delivery.
                claimed = true;
                break;
            }
            if (state->demand.compare_exchange_weak(
                    current, current - 1,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire))
            {
                claimed = true;
                break;
            }
            // current was refreshed by compare_exchange_weak; loop again.
        }
        if (!claimed)
        {
            // Demand was 0 (or raced to 0) — try the next subscriber.
            continue;
        }

        state->subscriber->onNext(std::move(payload));
        return vigine::Result{};  // payload moved; stop after first delivery
    }

    // No subscriber had demand — item is silently dropped (backpressure).
    return vigine::Result{};
}

vigine::Result ReactiveStream::complete()
{
    if (_impl->shutdownFlag.load(std::memory_order_acquire))
    {
        return vigine::Result{};
    }

    auto snapshot = _impl->snapshot();
    for (auto &state : snapshot)
    {
        bool alreadyTerminal  = state->terminal.exchange(true, std::memory_order_acq_rel);
        bool alreadyCancelled = state->cancelled.load(std::memory_order_acquire);
        if (!alreadyTerminal && !alreadyCancelled)
        {
            state->subscriber->onComplete();
        }
    }

    return vigine::Result{};
}

vigine::Result ReactiveStream::fail(vigine::Result error)
{
    if (_impl->shutdownFlag.load(std::memory_order_acquire))
    {
        return vigine::Result{};
    }

    auto snapshot = _impl->snapshot();
    for (auto &state : snapshot)
    {
        bool alreadyTerminal  = state->terminal.exchange(true, std::memory_order_acq_rel);
        bool alreadyCancelled = state->cancelled.load(std::memory_order_acquire);
        if (!alreadyTerminal && !alreadyCancelled)
        {
            state->subscriber->onError(error);
        }
    }

    return vigine::Result{};
}

vigine::Result ReactiveStream::shutdown()
{
    bool already = _impl->shutdownFlag.exchange(true, std::memory_order_acq_rel);
    if (already)
    {
        return vigine::Result{};
    }

    if (_impl->busToken)
    {
        _impl->busToken->cancel();
        _impl->busToken.reset();
    }

    std::vector<std::shared_ptr<SubscriptionState>> snapshot;
    {
        std::lock_guard<std::mutex> lk(_impl->registryMutex);
        for (auto &[id, state] : _impl->subscriptions)
        {
            snapshot.push_back(state);
        }
        _impl->subscriptions.clear();
    }

    for (auto &state : snapshot)
    {
        bool alreadyTerminal  = state->terminal.exchange(true, std::memory_order_acq_rel);
        bool alreadyCancelled = state->cancelled.load(std::memory_order_acquire);
        if (!alreadyTerminal && !alreadyCancelled)
        {
            state->subscriber->onComplete();
        }
    }

    return vigine::Result{};
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<IReactiveStream>
createReactiveStream(vigine::messaging::IMessageBus    &bus,
                     vigine::core::threading::IThreadManager &threadManager)
{
    return std::make_unique<ReactiveStream>(bus, threadManager);
}

} // namespace vigine::reactivestream

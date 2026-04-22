#include "vigine/requestbus/defaultrequestbus.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "vigine/messaging/imessage.h"
#include "vigine/messaging/imessagebus.h"
#include "vigine/messaging/imessagepayload.h"
#include "vigine/messaging/isubscriber.h"
#include "vigine/messaging/isubscriptiontoken.h"
#include "vigine/messaging/messagefilter.h"
#include "vigine/messaging/messagekind.h"
#include "vigine/messaging/routemode.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/requestbus/ifuture.h"
#include "vigine/requestbus/requestconfig.h"
#include "vigine/result.h"
#include "vigine/threading/ithreadmanager.h"
#include "vigine/threading/irunnable.h"
#include "vigine/topicbus/topicid.h"

namespace vigine::requestbus
{

namespace
{

// -----------------------------------------------------------------
// FutureState — shared state between DefaultFuture and the resolver.
//
// State machine (atomic):
//   Pending  -> Resolved  (reply arrives in time)
//   Pending  -> Expired   (TTL cleanup fires before reply)
//   Pending  -> Cancelled (IFuture::cancel called)
//
// Once in a terminal state the payload (if any) is owned here and
// moved to the IFuture::wait caller.
// -----------------------------------------------------------------

enum class FutureStatus : std::uint8_t
{
    Pending   = 0,
    Resolved  = 1,
    Expired   = 2,
    Cancelled = 3,
};

struct FutureState
{
    std::mutex                                           mutex;
    std::condition_variable                              cv;
    FutureStatus                                         status{FutureStatus::Pending};
    std::unique_ptr<vigine::messaging::IMessagePayload>  payload;

    // Returns true if we won the race to set a terminal state.
    bool tryResolve(std::unique_ptr<vigine::messaging::IMessagePayload> p)
    {
        std::unique_lock lk(mutex);
        if (status != FutureStatus::Pending)
        {
            return false;
        }
        payload = std::move(p);
        status  = FutureStatus::Resolved;
        cv.notify_all();
        return true;
    }

    bool tryExpire()
    {
        std::unique_lock lk(mutex);
        if (status != FutureStatus::Pending)
        {
            return false;
        }
        status = FutureStatus::Expired;
        cv.notify_all();
        return true;
    }

    bool tryCancel()
    {
        std::unique_lock lk(mutex);
        if (status != FutureStatus::Pending)
        {
            return false;
        }
        status = FutureStatus::Cancelled;
        cv.notify_all();
        return true;
    }
};

// -----------------------------------------------------------------
// DefaultFuture — IFuture handed to the caller of request().
// Holds a shared_ptr to FutureState; the bus's pending map holds the
// other shared_ptr half so either side can outlive the other safely.
// -----------------------------------------------------------------

class DefaultFuture final : public IFuture
{
  public:
    explicit DefaultFuture(std::shared_ptr<FutureState> state)
        : _state(std::move(state))
    {
    }

    [[nodiscard]] bool ready() const noexcept override
    {
        std::unique_lock lk(_state->mutex);
        return _state->status != FutureStatus::Pending;
    }

    [[nodiscard]] std::optional<std::unique_ptr<vigine::messaging::IMessagePayload>>
        wait(std::chrono::milliseconds timeout) override
    {
        std::unique_lock lk(_state->mutex);
        _state->cv.wait_for(lk, timeout,
            [this] { return _state->status != FutureStatus::Pending; });

        if (_state->status == FutureStatus::Resolved && _state->payload)
        {
            return std::move(_state->payload);
        }
        return std::nullopt;
    }

    void cancel() override
    {
        _state->tryCancel();
    }

  private:
    std::shared_ptr<FutureState> _state;
};

// -----------------------------------------------------------------
// RequestMessage — IMessage for MessageKind::TopicRequest (RPC ask).
// -----------------------------------------------------------------

class RequestPayloadWrapper final : public vigine::messaging::IMessagePayload
{
  public:
    explicit RequestPayloadWrapper(
        std::unique_ptr<vigine::messaging::IMessagePayload> inner) noexcept
        : _inner(std::move(inner))
    {
    }

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        return _inner ? _inner->typeId() : vigine::payload::PayloadTypeId{};
    }

    [[nodiscard]] const vigine::messaging::IMessagePayload *inner() const noexcept
    {
        return _inner.get();
    }

  private:
    std::unique_ptr<vigine::messaging::IMessagePayload> _inner;
};

class RequestMessage final : public vigine::messaging::IMessage
{
  public:
    RequestMessage(vigine::topicbus::TopicId                topic,
                   std::unique_ptr<RequestPayloadWrapper>   payload,
                   vigine::messaging::CorrelationId         corrId)
        : _topic(topic)
        , _payload(std::move(payload))
        , _corrId(corrId)
        , _scheduledFor(std::chrono::steady_clock::now())
    {
    }

    [[nodiscard]] vigine::messaging::MessageKind kind() const noexcept override
    {
        return vigine::messaging::MessageKind::TopicRequest;
    }

    [[nodiscard]] vigine::payload::PayloadTypeId payloadTypeId() const noexcept override
    {
        return _payload ? _payload->typeId() : vigine::payload::PayloadTypeId{};
    }

    [[nodiscard]] const vigine::messaging::IMessagePayload *payload() const noexcept override
    {
        return _payload.get();
    }

    [[nodiscard]] const vigine::messaging::AbstractMessageTarget *
        target() const noexcept override
    {
        return nullptr; // FirstMatch: no specific target needed
    }

    [[nodiscard]] vigine::messaging::RouteMode routeMode() const noexcept override
    {
        return vigine::messaging::RouteMode::FirstMatch;
    }

    [[nodiscard]] vigine::messaging::CorrelationId correlationId() const noexcept override
    {
        return _corrId;
    }

    [[nodiscard]] std::chrono::steady_clock::time_point
        scheduledFor() const noexcept override
    {
        return _scheduledFor;
    }

    // Internal accessor (this TU only) — lets the responder-side filtering
    // wrapper inspect the topic id stamped by request() so it can drop
    // messages addressed to other topics. Not part of any public header.
    [[nodiscard]] vigine::topicbus::TopicId topic() const noexcept
    {
        return _topic;
    }

  private:
    vigine::topicbus::TopicId                   _topic;
    std::unique_ptr<RequestPayloadWrapper>       _payload;
    vigine::messaging::CorrelationId             _corrId;
    std::chrono::steady_clock::time_point        _scheduledFor;
};

// -----------------------------------------------------------------
// TopicFilteringSubscriber — responder-side wrapper that inspects the
// RequestMessage's topic id and only forwards messages whose topic
// matches the one the caller passed to respondTo(). Without this
// wrapper, the raw message bus has no topic filter and every responder
// would receive every TopicRequest regardless of topic.
// -----------------------------------------------------------------

class TopicFilteringSubscriber final : public vigine::messaging::ISubscriber
{
  public:
    TopicFilteringSubscriber(vigine::topicbus::TopicId       topic,
                             vigine::messaging::ISubscriber *inner) noexcept
        : _topic(topic), _inner(inner)
    {
    }

    [[nodiscard]] vigine::messaging::DispatchResult
        onMessage(const vigine::messaging::IMessage &msg) override
    {
        if (!_inner)
        {
            return vigine::messaging::DispatchResult::Pass;
        }

        // Only dispatch if the envelope is one we stamped and the topic
        // matches. Any other message shape (or a mismatched topic) is
        // passed through so the bus keeps walking its FirstMatch chain.
        const auto *req = dynamic_cast<const RequestMessage *>(&msg);
        if (!req || req->topic() != _topic)
        {
            return vigine::messaging::DispatchResult::Pass;
        }

        return _inner->onMessage(msg);
    }

  private:
    vigine::topicbus::TopicId        _topic;
    vigine::messaging::ISubscriber  *_inner;
};

// -----------------------------------------------------------------
// FilteringSubscriptionToken — composite RAII handle that owns both
// the filtering wrapper and the inner bus subscription token. Member
// destruction order matters: _innerToken is declared first so it is
// destroyed LAST. The destructor cancels _innerToken explicitly before
// the wrapper is released, so the bus has stopped dispatching to the
// wrapper before the wrapper goes away.
// -----------------------------------------------------------------

class FilteringSubscriptionToken final : public vigine::messaging::ISubscriptionToken
{
  public:
    FilteringSubscriptionToken(
        std::unique_ptr<TopicFilteringSubscriber>              wrapper,
        std::unique_ptr<vigine::messaging::ISubscriptionToken> innerToken) noexcept
        : _wrapper(std::move(wrapper))
        , _innerToken(std::move(innerToken))
    {
    }

    ~FilteringSubscriptionToken() override
    {
        // Tear down the bus subscription first so no further dispatch
        // targets our wrapper, then allow the wrapper to be freed.
        if (_innerToken)
        {
            _innerToken->cancel();
        }
    }

    void cancel() noexcept override
    {
        if (_innerToken)
        {
            _innerToken->cancel();
        }
    }

    [[nodiscard]] bool active() const noexcept override
    {
        return _innerToken && _innerToken->active();
    }

  private:
    // Keep the wrapper alive as long as the bus may dispatch to it.
    std::unique_ptr<TopicFilteringSubscriber>              _wrapper;
    std::unique_ptr<vigine::messaging::ISubscriptionToken> _innerToken;
};

// -----------------------------------------------------------------
// ReplyMessage — IMessage for MessageKind::TopicPublish carrying the
// response payload back to the internal reply subscriber.
// -----------------------------------------------------------------

class ReplyPayloadWrapper final : public vigine::messaging::IMessagePayload
{
  public:
    explicit ReplyPayloadWrapper(
        std::unique_ptr<vigine::messaging::IMessagePayload> inner) noexcept
        : _inner(std::move(inner))
    {
    }

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        return _inner ? _inner->typeId() : vigine::payload::PayloadTypeId{};
    }

    [[nodiscard]] std::unique_ptr<vigine::messaging::IMessagePayload> takeInner() noexcept
    {
        return std::move(_inner);
    }

  private:
    std::unique_ptr<vigine::messaging::IMessagePayload> _inner;
};

class ReplyMessage final : public vigine::messaging::IMessage
{
  public:
    ReplyMessage(std::unique_ptr<ReplyPayloadWrapper>   payload,
                 vigine::messaging::CorrelationId       corrId)
        : _payload(std::move(payload))
        , _corrId(corrId)
        , _scheduledFor(std::chrono::steady_clock::now())
    {
    }

    [[nodiscard]] vigine::messaging::MessageKind kind() const noexcept override
    {
        return vigine::messaging::MessageKind::TopicPublish;
    }

    [[nodiscard]] vigine::payload::PayloadTypeId payloadTypeId() const noexcept override
    {
        return _payload ? _payload->typeId() : vigine::payload::PayloadTypeId{};
    }

    [[nodiscard]] const vigine::messaging::IMessagePayload *payload() const noexcept override
    {
        return _payload.get();
    }

    [[nodiscard]] const vigine::messaging::AbstractMessageTarget *
        target() const noexcept override
    {
        return nullptr;
    }

    [[nodiscard]] vigine::messaging::RouteMode routeMode() const noexcept override
    {
        return vigine::messaging::RouteMode::FanOut;
    }

    [[nodiscard]] vigine::messaging::CorrelationId correlationId() const noexcept override
    {
        return _corrId;
    }

    [[nodiscard]] std::chrono::steady_clock::time_point
        scheduledFor() const noexcept override
    {
        return _scheduledFor;
    }

    [[nodiscard]] ReplyPayloadWrapper *mutablePayload() noexcept
    {
        return _payload.get();
    }

  private:
    std::unique_ptr<ReplyPayloadWrapper>         _payload;
    vigine::messaging::CorrelationId             _corrId;
    std::chrono::steady_clock::time_point        _scheduledFor;
};

// -----------------------------------------------------------------
// TTL cleanup IRunnable — posted via IThreadManager after the effective
// TTL elapses. Removes the correlation entry (if still pending) and
// logs at debug.
// -----------------------------------------------------------------

class TtlCleanupRunnable final : public vigine::threading::IRunnable
{
  public:
    TtlCleanupRunnable(std::shared_ptr<FutureState>           state,
                       std::chrono::milliseconds              delay)
        : _state(std::move(state))
        , _delay(delay)
    {
    }

    [[nodiscard]] vigine::Result run() override
    {
        std::this_thread::sleep_for(_delay);
        _state->tryExpire();
        return vigine::Result{vigine::Result::Code::Success};
    }

  private:
    std::shared_ptr<FutureState>  _state;
    std::chrono::milliseconds     _delay;
};

} // namespace

// -----------------------------------------------------------------
// DefaultRequestBus::Impl
// -----------------------------------------------------------------

struct DefaultRequestBus::Impl
{
    vigine::threading::IThreadManager &threadManager;

    std::atomic<std::uint64_t>   corrCounter{1};
    std::atomic<bool>            shutdown{false};

    // Pending correlation map: corrId -> shared FutureState.
    std::mutex                                                         pendingMutex;
    std::unordered_map<std::uint64_t, std::shared_ptr<FutureState>>   pending;

    // Internal reply subscriber — listens for TopicPublish with known corrIds.
    // Declared separately to break the circular reference that would arise if
    // Impl owned the token (impl -> subscriber -> token -> bus -> impl).
    struct ReplySubscriber final : public vigine::messaging::ISubscriber
    {
        Impl *owner{nullptr};

        [[nodiscard]] vigine::messaging::DispatchResult
            onMessage(const vigine::messaging::IMessage &msg) override
        {
            if (!owner)
            {
                return vigine::messaging::DispatchResult::Pass;
            }

            const auto corrId = msg.correlationId();
            if (!corrId.valid())
            {
                return vigine::messaging::DispatchResult::Pass;
            }

            // Extract the reply payload from the message.
            const auto *wrapper =
                dynamic_cast<const ReplyPayloadWrapper *>(msg.payload());
            if (!wrapper)
            {
                // Not a reply we wrapped -- pass through.
                return vigine::messaging::DispatchResult::Pass;
            }

            // Find and remove the pending entry.
            std::shared_ptr<FutureState> state;
            {
                std::unique_lock lk(owner->pendingMutex);
                auto it = owner->pending.find(corrId.value);
                if (it == owner->pending.end())
                {
                    // Expired or already responded.
                    return vigine::messaging::DispatchResult::Pass;
                }
                state = it->second;
                owner->pending.erase(it);
            }

            // We need a non-const pointer to take the payload out.
            // The bus passes a const& to the message, but we need the
            // payload ownership: reconstruct from the wrapper's inner.
            // Since the bus owns the message we cannot move out of it
            // through the const API -- the reply payload was wrapped in
            // a non-const wrapper that `respond()` constructs.
            // Use the const_cast path only on our own internal object.
            auto *mutableWrapper =
                const_cast<ReplyPayloadWrapper *>(wrapper);
            auto innerPayload = mutableWrapper->takeInner();

            state->tryResolve(std::move(innerPayload));
            return vigine::messaging::DispatchResult::Handled;
        }
    };

    ReplySubscriber                                    replySubscriber;
    std::unique_ptr<vigine::messaging::ISubscriptionToken> replyToken;

    explicit Impl(vigine::threading::IThreadManager &tm) : threadManager(tm)
    {
        replySubscriber.owner = this;
    }
};

// -----------------------------------------------------------------
// DefaultRequestBus
// -----------------------------------------------------------------

DefaultRequestBus::DefaultRequestBus(vigine::messaging::IMessageBus    &bus,
                                     vigine::threading::IThreadManager  &threadManager)
    : AbstractRequestBus{bus}
    , _impl(std::make_unique<Impl>(threadManager))
{
    // Subscribe the internal reply listener for TopicPublish (FanOut).
    vigine::messaging::MessageFilter filter{};
    filter.kind          = vigine::messaging::MessageKind::TopicPublish;
    filter.expectedRoute = vigine::messaging::RouteMode::FanOut;
    _impl->replyToken    = bus.subscribe(filter, &_impl->replySubscriber);
}

DefaultRequestBus::~DefaultRequestBus()
{
    shutdown();
}

std::unique_ptr<IFuture>
DefaultRequestBus::request(vigine::topicbus::TopicId                           topic,
                            std::unique_ptr<vigine::messaging::IMessagePayload> payload,
                            const RequestConfig                                &cfg)
{
    if (!payload)
    {
        return nullptr;
    }

    if (_impl->shutdown.load(std::memory_order_acquire))
    {
        return nullptr;
    }

    // Stamp a unique correlation id.
    const std::uint64_t raw = _impl->corrCounter.fetch_add(1, std::memory_order_relaxed);
    const vigine::messaging::CorrelationId corrId{raw};

    // Create shared promise state.
    auto state = std::make_shared<FutureState>();

    {
        std::unique_lock lk(_impl->pendingMutex);
        _impl->pending[raw] = state;
    }

    // Post the request message to the bus. If the post fails (queue
    // full, bus closed, etc.) the responder will never receive the
    // request, so the caller's future would otherwise wait forever.
    // Expire the state synchronously and drop the pending entry so the
    // future's `wait()` returns the Expired terminal state right away
    // instead of hanging until the TTL cleanup runs.
    auto wrapped = std::make_unique<RequestPayloadWrapper>(std::move(payload));
    auto msg     = std::make_unique<RequestMessage>(topic, std::move(wrapped), corrId);
    const vigine::Result posted = bus().post(std::move(msg));
    if (!posted.isSuccess())
    {
        {
            std::unique_lock lk(_impl->pendingMutex);
            _impl->pending.erase(raw);
        }
        state->tryExpire();
        return std::make_unique<DefaultFuture>(std::move(state));
    }

    // Schedule TTL cleanup.
    const auto effectiveTtl =
        (cfg.ttl == std::chrono::milliseconds::zero())
            ? (cfg.timeout == std::chrono::milliseconds::max()
                   ? std::chrono::milliseconds{10000}  // fallback: 10s when timeout=infinity
                   : cfg.timeout * 2)
            : cfg.ttl;

    auto cleanupRunnable =
        std::make_unique<TtlCleanupRunnable>(state, effectiveTtl);
    (void)_impl->threadManager.schedule(std::move(cleanupRunnable));

    return std::make_unique<DefaultFuture>(std::move(state));
}

std::unique_ptr<vigine::messaging::ISubscriptionToken>
DefaultRequestBus::respondTo(vigine::topicbus::TopicId              topic,
                               vigine::messaging::ISubscriber        *subscriber)
{
    if (!subscriber)
    {
        return nullptr;
    }

    if (!topic.valid())
    {
        return nullptr;
    }

    if (_impl->shutdown.load(std::memory_order_acquire))
    {
        return nullptr;
    }

    vigine::messaging::MessageFilter filter{};
    filter.kind          = vigine::messaging::MessageKind::TopicRequest;
    filter.expectedRoute = vigine::messaging::RouteMode::FirstMatch;

    // Topic routing happens at the subscriber level: the raw bus has no
    // dedicated topic filter, so we install a small filtering wrapper
    // that inspects each incoming RequestMessage's topic id and only
    // forwards messages whose topic matches the one the caller asked
    // for. The composite token below keeps the wrapper alive as long as
    // the bus may dispatch to it, tearing the bus subscription down
    // first on destruction.
    auto wrapper = std::make_unique<TopicFilteringSubscriber>(topic, subscriber);
    auto innerToken = bus().subscribe(filter, wrapper.get());
    if (!innerToken)
    {
        return nullptr;
    }

    return std::make_unique<FilteringSubscriptionToken>(
        std::move(wrapper), std::move(innerToken));
}

void DefaultRequestBus::respond(
    vigine::messaging::CorrelationId                    corrId,
    std::unique_ptr<vigine::messaging::IMessagePayload> payload)
{
    if (!corrId.valid() || !payload)
    {
        return;
    }

    if (_impl->shutdown.load(std::memory_order_acquire))
    {
        return;
    }

    // Wrap the reply and post it so the internal ReplySubscriber picks it up.
    //
    // Design note on discriminator choice:
    //   ReplyMessage uses MessageKind::TopicPublish + RouteMode::FanOut, which
    //   is the same shape as regular topic-bus traffic. That is intentional --
    //   the reply is not distinguished by MessageKind or RouteMode, but by the
    //   payload wrapper type. Only ReplyPayloadWrapper-typed payloads are
    //   considered replies by the bus's internal ReplySubscriber (see
    //   ReplySubscriber::onMessage, which performs dynamic_cast<const
    //   ReplyPayloadWrapper*> and returns DispatchResult::Pass if the cast
    //   fails). A third-party subscriber listening on the same bus that
    //   receives this message will see an IMessagePayload whose concrete type
    //   is ReplyPayloadWrapper -- a type it cannot meaningfully interpret
    //   (no public accessor for the inner payload from outside this TU), so
    //   it will ignore it. Conversely, the reply subscriber will refuse any
    //   message whose payload is not a ReplyPayloadWrapper. This makes the
    //   wrapper the sole discriminator for reply traffic; the TopicPublish
    //   kind is shared with unrelated publishes on purpose so the reply
    //   flows through the same dispatch path without a dedicated message
    //   kind enumerant.
    auto wrapped = std::make_unique<ReplyPayloadWrapper>(std::move(payload));
    auto msg     = std::make_unique<ReplyMessage>(std::move(wrapped), corrId);
    (void)bus().post(std::move(msg));
}

vigine::Result DefaultRequestBus::shutdown()
{
    bool expected = false;
    if (!_impl->shutdown.compare_exchange_strong(
            expected, true,
            std::memory_order_acq_rel,
            std::memory_order_acquire))
    {
        return vigine::Result{vigine::Result::Code::Success};
    }

    // Cancel every pending future.
    {
        std::unique_lock lk(_impl->pendingMutex);
        for (auto &[key, state] : _impl->pending)
        {
            state->tryCancel();
        }
        _impl->pending.clear();
    }

    // Release the internal reply subscription.
    if (_impl->replyToken)
    {
        _impl->replyToken->cancel();
        _impl->replyToken.reset();
    }

    return vigine::Result{vigine::Result::Code::Success};
}

// -----------------------------------------------------------------
// Factory
// -----------------------------------------------------------------

std::unique_ptr<IRequestBus>
createRequestBus(vigine::messaging::IMessageBus    &bus,
                 vigine::threading::IThreadManager &threadManager)
{
    return std::make_unique<DefaultRequestBus>(bus, threadManager);
}

} // namespace vigine::requestbus

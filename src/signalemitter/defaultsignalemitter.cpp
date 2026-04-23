#include "vigine/signalemitter/defaultsignalemitter.h"

#include <cassert>
#include <chrono>
#include <memory>
#include <utility>

#include "vigine/messaging/abstractmessagetarget.h"
#include "vigine/messaging/busconfig.h"
#include "vigine/messaging/imessage.h"
#include "vigine/messaging/imessagepayload.h"
#include "vigine/messaging/messagekind.h"
#include "vigine/messaging/routemode.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/result.h"
#include "vigine/signalemitter/isignalpayload.h"
#include "vigine/threading/ithreadmanager.h"

namespace vigine::signalemitter
{

namespace
{

// -----------------------------------------------------------------
// SignalMessage — concrete IMessage carrying an ISignalPayload.
// Private to this translation unit; never visible to callers.
// -----------------------------------------------------------------

class SignalMessage final : public vigine::messaging::IMessage
{
  public:
    SignalMessage(std::unique_ptr<ISignalPayload>                  payload,
                  const vigine::messaging::AbstractMessageTarget  *target,
                  vigine::messaging::RouteMode                     routeMode)
        : _payloadTypeId(payload ? payload->typeId() : vigine::payload::PayloadTypeId{})
        , _payload(std::move(payload))
        , _target(target)
        , _routeMode(routeMode)
        , _scheduledFor(std::chrono::steady_clock::now())
    {
    }

    [[nodiscard]] vigine::messaging::MessageKind kind() const noexcept override
    {
        return vigine::messaging::MessageKind::Signal;
    }

    [[nodiscard]] vigine::payload::PayloadTypeId
        payloadTypeId() const noexcept override
    {
        return _payloadTypeId;
    }

    [[nodiscard]] const vigine::messaging::IMessagePayload *
        payload() const noexcept override
    {
        return _payload.get();
    }

    [[nodiscard]] const vigine::messaging::AbstractMessageTarget *
        target() const noexcept override
    {
        return _target;
    }

    [[nodiscard]] vigine::messaging::RouteMode
        routeMode() const noexcept override
    {
        return _routeMode;
    }

    [[nodiscard]] vigine::messaging::CorrelationId
        correlationId() const noexcept override
    {
        return vigine::messaging::CorrelationId{};
    }

    [[nodiscard]] std::chrono::steady_clock::time_point
        scheduledFor() const noexcept override
    {
        return _scheduledFor;
    }

  private:
    vigine::payload::PayloadTypeId                          _payloadTypeId;
    std::unique_ptr<ISignalPayload>                         _payload;
    const vigine::messaging::AbstractMessageTarget         *_target;
    vigine::messaging::RouteMode                            _routeMode;
    std::chrono::steady_clock::time_point                   _scheduledFor;
};

// -----------------------------------------------------------------
// Default BusConfig for an inline-only signal bus.
// InlineOnly keeps dispatch synchronous on the caller's thread so
// the emitter is usable without a dedicated worker thread.
// -----------------------------------------------------------------

[[nodiscard]] vigine::messaging::BusConfig inlineBusConfig() noexcept
{
    return vigine::messaging::BusConfig{
        /* id           */ vigine::messaging::BusId{},
        /* name         */ std::string_view{"signal-emitter-bus"},
        /* priority     */ vigine::messaging::BusPriority::Normal,
        /* threading    */ vigine::messaging::ThreadingPolicy::InlineOnly,
        /* capacity     */ vigine::messaging::QueueCapacity{256, true},
        /* backpressure */ vigine::messaging::BackpressurePolicy::Error,
    };
}

} // namespace

// -----------------------------------------------------------------
// sharedBusConfig — public sibling of inlineBusConfig.
// Same shape as the inline default, but uses ThreadingPolicy::Shared
// so dispatch lands on the thread manager's shared worker pool.
// The distinct name keeps diagnostics unambiguous.
// -----------------------------------------------------------------

vigine::messaging::BusConfig sharedBusConfig() noexcept
{
    return vigine::messaging::BusConfig{
        /* id           */ vigine::messaging::BusId{},
        /* name         */ std::string_view{"signal-emitter-bus-shared"},
        /* priority     */ vigine::messaging::BusPriority::Normal,
        /* threading    */ vigine::messaging::ThreadingPolicy::Shared,
        /* capacity     */ vigine::messaging::QueueCapacity{256, true},
        /* backpressure */ vigine::messaging::BackpressurePolicy::Error,
    };
}

// -----------------------------------------------------------------
// DefaultSignalEmitter
// -----------------------------------------------------------------

DefaultSignalEmitter::DefaultSignalEmitter(
    vigine::threading::IThreadManager &threadManager)
    : AbstractSignalEmitter{inlineBusConfig(), threadManager}
{
}

DefaultSignalEmitter::DefaultSignalEmitter(
    vigine::threading::IThreadManager &threadManager,
    vigine::messaging::BusConfig       config)
    : AbstractSignalEmitter{std::move(config), threadManager}
{
}

vigine::Result
DefaultSignalEmitter::emit(std::unique_ptr<ISignalPayload> payload)
{
    if (!payload)
    {
        return vigine::Result{vigine::Result::Code::Error, "emit: null payload"};
    }
    auto msg = std::make_unique<SignalMessage>(
        std::move(payload),
        /*target=*/nullptr,
        vigine::messaging::RouteMode::FanOut);
    return vigine::messaging::AbstractMessageBus::post(std::move(msg));
}

vigine::Result
DefaultSignalEmitter::emitTo(
    const vigine::messaging::AbstractMessageTarget *target,
    std::unique_ptr<ISignalPayload>                 payload)
{
    if (!target)
    {
        return vigine::Result{vigine::Result::Code::Error, "emitTo: null target"};
    }
    if (!payload)
    {
        return vigine::Result{vigine::Result::Code::Error, "emitTo: null payload"};
    }
    auto msg = std::make_unique<SignalMessage>(
        std::move(payload),
        target,
        vigine::messaging::RouteMode::FirstMatch);
    return vigine::messaging::AbstractMessageBus::post(std::move(msg));
}

std::unique_ptr<vigine::messaging::ISubscriptionToken>
DefaultSignalEmitter::subscribeSignal(
    vigine::messaging::MessageFilter  filter,
    vigine::messaging::ISubscriber   *subscriber)
{
    return AbstractSignalEmitter::subscribeSignal(filter, subscriber);
}

// -----------------------------------------------------------------
// Factory
// -----------------------------------------------------------------

std::unique_ptr<ISignalEmitter>
createSignalEmitter(vigine::threading::IThreadManager &threadManager)
{
    return std::make_unique<DefaultSignalEmitter>(threadManager);
}

std::unique_ptr<ISignalEmitter>
createSignalEmitter(vigine::threading::IThreadManager &threadManager,
                    vigine::messaging::BusConfig       config)
{
    return std::make_unique<DefaultSignalEmitter>(threadManager, std::move(config));
}

} // namespace vigine::signalemitter

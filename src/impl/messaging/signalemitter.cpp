#include "vigine/impl/messaging/signalemitter.h"

#include <cassert>
#include <chrono>
#include <memory>
#include <utility>

#include "vigine/api/messaging/abstractmessagetarget.h"
#include "vigine/api/messaging/busconfig.h"
#include "vigine/api/messaging/imessage.h"
#include "vigine/api/messaging/imessagepayload.h"
#include "vigine/api/messaging/messagekind.h"
#include "vigine/api/messaging/routemode.h"
#include "vigine/api/messaging/payload/ipayloadregistry.h"
#include "vigine/api/messaging/payload/payloadtypeid.h"
#include "vigine/result.h"
#include "vigine/api/messaging/payload/isignalpayload.h"
#include "vigine/core/threading/ithreadmanager.h"

#include <sstream>

namespace vigine::messaging
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
// Same shape as the inline default but with ThreadingPolicy::Shared
// and a distinct name for diagnostics. Note: the `Shared` policy is
// defined as "dispatch on a bus worker", but the current
// AbstractMessageBus::post implementation drains Shared queues on the
// posting thread (the bus worker pump is deferred). Callers who need
// a real thread hop must either post from the desired thread already
// or combine this config with an IThreadManager::schedule on the
// producer side. TaskFlow::signal handles that automatically for
// non-Any ThreadAffinity values.
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
// SignalEmitter
// -----------------------------------------------------------------

SignalEmitter::SignalEmitter(
    vigine::core::threading::IThreadManager &threadManager)
    : AbstractSignalEmitter{inlineBusConfig(), threadManager}
{
}

SignalEmitter::SignalEmitter(
    vigine::core::threading::IThreadManager &threadManager,
    vigine::messaging::BusConfig       config)
    : AbstractSignalEmitter{std::move(config), threadManager}
{
}

SignalEmitter::SignalEmitter(
    vigine::core::threading::IThreadManager &threadManager,
    vigine::messaging::BusConfig             config,
    vigine::payload::IPayloadRegistry       &registry)
    : AbstractSignalEmitter{std::move(config), threadManager}
    , _payloadRegistry{&registry}
{
}

namespace
{
// Build a hex-formatted "0xNNNN" diagnostic for an unregistered id.
[[nodiscard]] std::string formatUnregisteredId(
    const char                            *prefix,
    vigine::payload::PayloadTypeId         id)
{
    std::ostringstream out;
    out << prefix << ": payload type id 0x" << std::hex << id.value
        << " is not registered in the payload registry";
    return out.str();
}
} // namespace

vigine::Result
SignalEmitter::emit(std::unique_ptr<ISignalPayload> payload)
{
    if (!payload)
    {
        return vigine::Result{vigine::Result::Code::Error, "emit: null payload"};
    }
    if (_payloadRegistry != nullptr &&
        !_payloadRegistry->isRegistered(payload->typeId()))
    {
        return vigine::Result{vigine::Result::Code::Error,
                              formatUnregisteredId("emit", payload->typeId())};
    }
    auto msg = std::make_unique<SignalMessage>(
        std::move(payload),
        /*target=*/nullptr,
        vigine::messaging::RouteMode::FanOut);
    return vigine::messaging::AbstractMessageBus::post(std::move(msg));
}

vigine::Result
SignalEmitter::emitTo(
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
    if (_payloadRegistry != nullptr &&
        !_payloadRegistry->isRegistered(payload->typeId()))
    {
        return vigine::Result{vigine::Result::Code::Error,
                              formatUnregisteredId("emitTo", payload->typeId())};
    }
    auto msg = std::make_unique<SignalMessage>(
        std::move(payload),
        target,
        vigine::messaging::RouteMode::FirstMatch);
    return vigine::messaging::AbstractMessageBus::post(std::move(msg));
}

std::unique_ptr<vigine::messaging::ISubscriptionToken>
SignalEmitter::subscribeSignal(
    vigine::messaging::MessageFilter  filter,
    vigine::messaging::ISubscriber   *subscriber)
{
    return AbstractSignalEmitter::subscribeSignal(filter, subscriber);
}

// -----------------------------------------------------------------
// Factory
// -----------------------------------------------------------------

std::unique_ptr<ISignalEmitter>
createSignalEmitter(vigine::core::threading::IThreadManager &threadManager)
{
    return std::make_unique<SignalEmitter>(threadManager);
}

std::unique_ptr<ISignalEmitter>
createSignalEmitter(vigine::core::threading::IThreadManager &threadManager,
                    vigine::messaging::BusConfig       config)
{
    return std::make_unique<SignalEmitter>(threadManager, std::move(config));
}

std::unique_ptr<ISignalEmitter>
createSignalEmitter(vigine::core::threading::IThreadManager &threadManager,
                    vigine::messaging::BusConfig             config,
                    vigine::payload::IPayloadRegistry       &registry)
{
    return std::make_unique<SignalEmitter>(threadManager, std::move(config), registry);
}

} // namespace vigine::messaging

#pragma once

// ---------------------------------------------------------------------------
// Narrow helpers shared across the full-contract scenarios.
//
// The helpers live in fixtures/ (not in each scenario .cpp) because every
// scenario that talks to IMessageBus / facades needs the same three
// minimal concrete types -- a payload, a message envelope, and a
// counting subscriber -- and duplicating them would drown the scenario
// files in boilerplate.
//
// Every helper has strict encapsulation (private data, public methods).
// No templates in the public surface; each type is a concrete final
// class suitable for direct std::make_unique<Type>(...) construction.
// ---------------------------------------------------------------------------

#include "vigine/messaging/abstractmessagetarget.h"
#include "vigine/messaging/iconnectiontoken.h"
#include "vigine/messaging/imessage.h"
#include "vigine/messaging/imessagepayload.h"
#include "vigine/messaging/isubscriber.h"
#include "vigine/messaging/messagekind.h"
#include "vigine/messaging/routemode.h"
#include "vigine/messaging/targetkind.h"
#include "vigine/payload/payloadtypeid.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <utility>

namespace vigine::contract
{

/**
 * @brief Minimal concrete @ref vigine::messaging::IMessagePayload carrying
 *        only the type id -- enough for the router to filter on.
 */
class ContractPayload final : public vigine::messaging::IMessagePayload
{
  public:
    explicit ContractPayload(vigine::payload::PayloadTypeId id) noexcept : _id(id) {}

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        return _id;
    }

  private:
    vigine::payload::PayloadTypeId _id;
};

/**
 * @brief Minimal concrete @ref vigine::messaging::IMessage envelope.
 *
 * Carries one @ref ContractPayload and the routing tuple (kind + route
 * mode + optional target) supplied at construction. Correlation id and
 * scheduled-for are default-initialised; scenarios that need either
 * field wrap their own envelope type.
 */
class ContractMessage final : public vigine::messaging::IMessage
{
  public:
    ContractMessage(vigine::messaging::MessageKind             kind,
                    vigine::messaging::RouteMode               route,
                    vigine::payload::PayloadTypeId             typeId,
                    const vigine::messaging::AbstractMessageTarget *target = nullptr) noexcept
        : _kind(kind)
        , _route(route)
        , _typeId(typeId)
        , _target(target)
        , _payload(std::make_unique<ContractPayload>(typeId))
    {
    }

    [[nodiscard]] vigine::messaging::MessageKind kind() const noexcept override
    {
        return _kind;
    }

    [[nodiscard]] vigine::payload::PayloadTypeId
        payloadTypeId() const noexcept override
    {
        return _typeId;
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

    [[nodiscard]] vigine::messaging::RouteMode routeMode() const noexcept override
    {
        return _route;
    }

    [[nodiscard]] vigine::messaging::CorrelationId correlationId() const noexcept override
    {
        return vigine::messaging::CorrelationId{};
    }

    [[nodiscard]] std::chrono::steady_clock::time_point
        scheduledFor() const noexcept override
    {
        return std::chrono::steady_clock::time_point{};
    }

  private:
    vigine::messaging::MessageKind                     _kind;
    vigine::messaging::RouteMode                       _route;
    vigine::payload::PayloadTypeId                     _typeId;
    const vigine::messaging::AbstractMessageTarget    *_target;
    std::unique_ptr<ContractPayload>                   _payload;
};

/**
 * @brief Minimal concrete @ref vigine::messaging::ISubscriber that counts
 *        onMessage invocations and returns the configured DispatchResult.
 *
 * Atomics let two threads observe the counter without a data race; the
 * contract suite uses that to verify delivery after a fan-out post.
 */
class CountingSubscriber final : public vigine::messaging::ISubscriber
{
  public:
    explicit CountingSubscriber(
        vigine::messaging::DispatchResult reply =
            vigine::messaging::DispatchResult::Handled) noexcept
        : _reply(reply)
    {
    }

    [[nodiscard]] vigine::messaging::DispatchResult
        onMessage(const vigine::messaging::IMessage & /*message*/) override
    {
        _hits.fetch_add(1, std::memory_order_acq_rel);
        return _reply;
    }

    [[nodiscard]] std::uint32_t hits() const noexcept
    {
        return _hits.load(std::memory_order_acquire);
    }

  private:
    vigine::messaging::DispatchResult _reply;
    std::atomic<std::uint32_t>        _hits{0};
};

/**
 * @brief Minimal concrete @ref vigine::messaging::AbstractMessageTarget
 *        that counts messages delivered through its onMessage hook.
 *
 * Needed by scenarios that exercise IEventScheduler (which routes
 * MessageKind::Event to a target) and by scenarios that register a
 * target on the bus via registerTarget().
 */
class CountingTarget final : public vigine::messaging::AbstractMessageTarget
{
  public:
    [[nodiscard]] vigine::messaging::TargetKind targetKind() const noexcept override
    {
        return vigine::messaging::TargetKind::User;
    }

    void onMessage(const vigine::messaging::IMessage & /*message*/) override
    {
        _count.fetch_add(1, std::memory_order_acq_rel);
    }

    [[nodiscard]] std::uint32_t count() const noexcept
    {
        return _count.load(std::memory_order_acquire);
    }

  private:
    std::atomic<std::uint32_t> _count{0};
};

} // namespace vigine::contract

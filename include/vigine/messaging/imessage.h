#pragma once

#include <chrono>
#include <cstdint>

#include "vigine/messaging/messagekind.h"
#include "vigine/messaging/routemode.h"
#include "vigine/payload/payloadtypeid.h"

namespace vigine::messaging
{
class AbstractMessageTarget;
class IMessagePayload;

/**
 * @brief Opaque identifier pairing a request with its response.
 *
 * Carried on every @ref IMessage. A zero value means "uncorrelated" --
 * the message does not pair with a prior request. Facades that need
 * request/response matching (for example @c TopicRequest) allocate a
 * fresh non-zero value per pair.
 */
struct CorrelationId
{
    std::uint64_t value{0};

    [[nodiscard]] constexpr bool valid() const noexcept { return value != 0; }

    [[nodiscard]] friend constexpr bool operator==(CorrelationId lhs,
                                                   CorrelationId rhs) noexcept
    {
        return lhs.value == rhs.value;
    }

    [[nodiscard]] friend constexpr bool operator!=(CorrelationId lhs,
                                                   CorrelationId rhs) noexcept
    {
        return lhs.value != rhs.value;
    }
};

/**
 * @brief Pure-virtual envelope passed to @ref IMessageBus::post.
 *
 * An @ref IMessage bundles the routing metadata (kind + route mode +
 * optional target), the polymorphic payload, and the dispatch timestamp
 * used by scheduled delivery paths. The bus takes unique ownership at
 * @c post time; implementations are immutable for the lifetime of the
 * message so dispatch workers never need to take a lock on message
 * state.
 *
 * The target pointer is optional: when @c null, the message is addressed
 * to "anyone who matches the filter" and delivery defaults to the
 * broadcast semantics of the chosen @ref RouteMode. When non-null, the
 * pointer must remain valid until the bus has finished dispatching the
 * message -- which is the invariant provided by
 * @ref AbstractMessageTarget owning its tokens.
 */
class IMessage
{
  public:
    virtual ~IMessage() = default;

    /**
     * @brief Returns the closed @ref MessageKind classifying this
     *        envelope.
     *
     * Stable for the message's lifetime. The bus validates the value on
     * @ref IMessageBus::post and rejects out-of-enum values.
     */
    [[nodiscard]] virtual MessageKind kind() const noexcept = 0;

    /**
     * @brief Returns the closed @ref vigine::payload::PayloadTypeId that
     *        classifies the payload carried by this envelope.
     *
     * Mirrors @ref IMessagePayload::typeId so the bus can filter
     * subscriptions without dereferencing the payload pointer.
     */
    [[nodiscard]] virtual vigine::payload::PayloadTypeId
        payloadTypeId() const noexcept = 0;

    /**
     * @brief Returns the polymorphic payload, or @c nullptr when the
     *        envelope carries no payload (for example a @c Control
     *        message that only needs its kind).
     *
     * The returned pointer is owned by the message; callers must not
     * outlive the enclosing @ref IMessage.
     */
    [[nodiscard]] virtual const IMessagePayload *payload() const noexcept = 0;

    /**
     * @brief Returns the optional target this message is addressed to.
     *
     * When @c null, the message is not scoped to a particular target;
     * the bus applies the @ref RouteMode to the whole subscription
     * registry. When non-null, the pointer identifies the target and
     * must remain valid for the duration of the dispatch.
     */
    [[nodiscard]] virtual const AbstractMessageTarget *target() const noexcept = 0;

    /**
     * @brief Returns the @ref RouteMode the bus uses to walk the
     *        subscription registry.
     *
     * Stable for the message's lifetime. The bus validates the value on
     * @ref IMessageBus::post and rejects out-of-enum values.
     */
    [[nodiscard]] virtual RouteMode routeMode() const noexcept = 0;

    /**
     * @brief Returns the correlation id pairing a request with its
     *        response, or a zero-valued sentinel for uncorrelated
     *        traffic.
     */
    [[nodiscard]] virtual CorrelationId correlationId() const noexcept = 0;

    /**
     * @brief Returns the time point at which the bus is expected to
     *        deliver this message.
     *
     * A value in the past means "deliver as soon as possible". A value
     * in the future schedules delayed delivery; the bus requeues the
     * message until the monotonic clock catches up.
     */
    [[nodiscard]] virtual std::chrono::steady_clock::time_point
        scheduledFor() const noexcept = 0;

    IMessage(const IMessage &)            = delete;
    IMessage &operator=(const IMessage &) = delete;
    IMessage(IMessage &&)                 = delete;
    IMessage &operator=(IMessage &&)      = delete;

  protected:
    IMessage() = default;
};

} // namespace vigine::messaging

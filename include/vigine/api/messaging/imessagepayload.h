#pragma once

#include "vigine/api/messaging/payload/payloadtypeid.h"

namespace vigine::messaging
{
/**
 * @brief Pure-virtual polymorphic payload carried by an @ref IMessage.
 *
 * Every payload advertises its closed @ref vigine::payload::PayloadTypeId
 * so that the bus (and downstream facades) can route and downcast without
 * a @c dynamic_cast. The payload object is owned by its enclosing
 * @ref IMessage; callers never manage payload lifetime directly.
 *
 * Implementations are expected to be immutable for the lifetime of the
 * message: once a payload is handed to the bus, readers running on
 * dispatch workers observe the same bytes the poster recorded. Anything
 * mutable belongs outside the payload (for example in a separate
 * channel-owned state).
 *
 * Thread-safety: @ref typeId is safe to call from any thread. The
 * polymorphic state held by concrete subclasses follows the same
 * immutability contract: no locks are required on the dispatch hot path.
 */
class IMessagePayload
{
  public:
    virtual ~IMessagePayload() = default;

    /**
     * @brief Returns the closed @ref vigine::payload::PayloadTypeId that
     *        classifies this payload.
     *
     * Stable for the payload's lifetime. The bus treats the returned
     * value as the static type tag and relies on
     * @ref vigine::payload::IPayloadRegistry to validate it at publish
     * and subscribe time.
     */
    [[nodiscard]] virtual vigine::payload::PayloadTypeId typeId() const noexcept = 0;

    IMessagePayload(const IMessagePayload &)            = delete;
    IMessagePayload &operator=(const IMessagePayload &) = delete;
    IMessagePayload(IMessagePayload &&)                 = delete;
    IMessagePayload &operator=(IMessagePayload &&)      = delete;

  protected:
    IMessagePayload() = default;
};

} // namespace vigine::messaging

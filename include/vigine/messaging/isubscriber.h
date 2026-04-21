#pragma once

#include "vigine/messaging/routemode.h"

namespace vigine::messaging
{
class IMessage;

/**
 * @brief Pure-virtual subscription callback invoked by the bus on
 *        dispatch.
 *
 * Consumers of @ref IMessageBus::subscribe implement this interface to
 * receive messages. The bus owns the lifetime through an internal
 * registry entry; subscribers must remain valid until the matching
 * @ref ISubscriptionToken is released (which unregisters them).
 *
 * The callback reports back to the bus through @ref DispatchResult so
 * that the dispatch driver can decide whether to keep walking (for the
 * @c FirstMatch and @c Chain route modes) or to stop early. The bus
 * isolates exceptions thrown by @ref onMessage: an escaping exception
 * is logged and treated as @ref DispatchResult::Handled so a misbehaving
 * subscriber does not stall the whole registry.
 *
 * Thread-safety: @ref onMessage may be invoked from any worker thread
 * the bus chooses, but never concurrently for the same subscriber from
 * two different messages. Subscribers that touch shared state are
 * expected to provide their own synchronisation on that state.
 *
 * Reentrancy: an @ref onMessage implementation must not destroy its own
 * @ref ISubscriptionToken (or the enclosing subscriber). Doing so makes
 * the token destructor wait for the in-flight dispatch to complete,
 * which is the dispatch still running -- a self-deadlock.
 */
class ISubscriber
{
  public:
    virtual ~ISubscriber() = default;

    /**
     * @brief Delivers @p message to the subscriber.
     *
     * Implementations return a @ref DispatchResult so the bus can decide
     * whether to keep walking the registry. See @ref RouteMode for how
     * each route mode interprets the return value.
     */
    [[nodiscard]] virtual DispatchResult onMessage(const IMessage &message) = 0;

    ISubscriber(const ISubscriber &)            = delete;
    ISubscriber &operator=(const ISubscriber &) = delete;
    ISubscriber(ISubscriber &&)                 = delete;
    ISubscriber &operator=(ISubscriber &&)      = delete;

  protected:
    ISubscriber() = default;
};

} // namespace vigine::messaging

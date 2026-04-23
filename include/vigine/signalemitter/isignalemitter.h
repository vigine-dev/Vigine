#pragma once

#include <memory>

#include "vigine/messaging/messagefilter.h"
#include "vigine/messaging/isubscriber.h"
#include "vigine/messaging/isubscriptiontoken.h"
#include "vigine/result.h"
#include "vigine/signalemitter/isignalpayload.h"

namespace vigine::messaging
{
class AbstractMessageTarget;
} // namespace vigine::messaging

namespace vigine::signalemitter
{

/**
 * @brief Pure-virtual facade for the Signal dispatch pattern.
 *
 * @ref ISignalEmitter is the Level-2 facade over @ref vigine::messaging::IMessageBus
 * for the signal-slot pattern. It encapsulates the three operations that
 * signal producers and subscribers need:
 *
 *   - @ref emit            — broadcast a payload to all matching subscribers
 *                            on the bound bus
 *                            (@ref vigine::messaging::RouteMode::FanOut).
 *   - @ref emitTo          — target-scoped emit to subscribers registered
 *                            against a specific
 *                            @ref vigine::messaging::AbstractMessageTarget
 *                            (@ref vigine::messaging::RouteMode::FirstMatch).
 *   - @ref subscribeSignal — register an @ref vigine::messaging::ISubscriber
 *                            against a filter on the emitter's internal
 *                            bus; returns an
 *                            @ref vigine::messaging::ISubscriptionToken
 *                            that cancels the subscription when dropped.
 *
 * Exposing @ref subscribeSignal on the facade lets callers that only hold
 * an @c ISignalEmitter* register subscribers without reaching for a
 * separate @ref vigine::messaging::IMessageBus handle.
 *
 * Invariants:
 *   - INV-1: no template parameters in the public surface.
 *   - INV-10: @c I prefix for this pure-virtual interface (no state).
 *   - INV-11: no graph types (@ref vigine::graph::NodeId,
 *             @ref vigine::graph::INode, etc.) appear in this header.
 *   - FF-1: factory @ref createSignalEmitter returns @c std::unique_ptr.
 *
 * The concrete implementation (@ref DefaultSignalEmitter) is private to
 * @c src/signalemitter/ and unreachable from public callers.
 */
class ISignalEmitter
{
  public:
    virtual ~ISignalEmitter() = default;

    /**
     * @brief Broadcasts @p payload to every subscriber on the bound bus
     *        whose filter matches @ref vigine::messaging::MessageKind::Signal
     *        and the payload's @ref vigine::payload::PayloadTypeId.
     *
     * Ownership of @p payload is transferred to the bus on success. A
     * null @p payload returns an error @ref vigine::Result without
     * posting to the bus.
     *
     * Route mode: @ref vigine::messaging::RouteMode::FanOut — every
     * matching subscriber receives the message.
     */
    [[nodiscard]] virtual vigine::Result
        emit(std::unique_ptr<ISignalPayload> payload) = 0;

    /**
     * @brief Emits @p payload to subscribers registered against @p target
     *        on the bound bus.
     *
     * Ownership of @p payload is transferred to the bus on success. A
     * null @p payload or a null @p target returns an error
     * @ref vigine::Result without posting.
     *
     * Route mode: @ref vigine::messaging::RouteMode::FirstMatch — the
     * first subscriber registered against @p target receives the message.
     */
    [[nodiscard]] virtual vigine::Result
        emitTo(const vigine::messaging::AbstractMessageTarget *target,
               std::unique_ptr<ISignalPayload>                 payload) = 0;

    /**
     * @brief Subscribes @p subscriber to signals matching @p filter on the
     *        emitter's internal bus.
     *
     * The filter's @c kind field is forced to
     * @ref vigine::messaging::MessageKind::Signal before forwarding, so
     * the subscription is always scoped to signal traffic regardless of
     * what the caller placed there.
     *
     * Returns an @ref vigine::messaging::ISubscriptionToken; dropping the
     * token cancels the subscription. A null @p subscriber returns a null
     * token.
     */
    [[nodiscard]] virtual std::unique_ptr<vigine::messaging::ISubscriptionToken>
        subscribeSignal(vigine::messaging::MessageFilter  filter,
                        vigine::messaging::ISubscriber   *subscriber) = 0;

    ISignalEmitter(const ISignalEmitter &)            = delete;
    ISignalEmitter &operator=(const ISignalEmitter &) = delete;
    ISignalEmitter(ISignalEmitter &&)                 = delete;
    ISignalEmitter &operator=(ISignalEmitter &&)      = delete;

  protected:
    ISignalEmitter() = default;
};

} // namespace vigine::signalemitter

#pragma once

#include <memory>

#include "vigine/messaging/busconfig.h"
#include "vigine/messaging/busid.h"
#include "vigine/messaging/imessage.h"
#include "vigine/messaging/isubscriber.h"
#include "vigine/messaging/isubscriptiontoken.h"
#include "vigine/messaging/messagefilter.h"
#include "vigine/result.h"

namespace vigine::messaging
{
class AbstractMessageTarget;

/**
 * @brief Pure-virtual unified message-bus core.
 *
 * @ref IMessageBus is the level-1 wrapper every facade builds on top of.
 * It owns a subscription registry keyed on @ref AbstractMessageTarget
 * pointer identity and dispatches posted messages to matching
 * subscribers according to the @ref RouteMode carried on each message.
 * Exactly one bus shape is exposed publicly; the eight facades shipped
 * in plan_15..plan_22 wrap this same surface with use-case ergonomics.
 *
 * Ownership and lifetime:
 *   - @ref post takes unique ownership of the posted @ref IMessage. The
 *     bus keeps the envelope alive until every subscriber has finished
 *     processing it.
 *   - @ref subscribe returns a @c std::unique_ptr<ISubscriptionToken>.
 *     The token is the RAII handle to the subscription slot; dropping
 *     it or calling @ref ISubscriptionToken::cancel tears the slot down.
 *   - @ref registerTarget installs a target into the bus's
 *     control-block registry and hands the target back a
 *     @ref ConnectionToken through @ref AbstractMessageTarget::acceptConnection.
 *     The bus never owns the target; the target owns its tokens.
 *   - @ref shutdown drains every queued message, joins any worker
 *     threads owned by the bus, marks the control block dead, and
 *     makes subsequent @ref post calls return an error result.
 *
 * Thread-safety: every entry point is safe to call from any thread at
 * any time before @ref shutdown completes. After shutdown, posts fail
 * fast with an error @ref Result and subscriptions cancel themselves.
 *
 * INV-1 and INV-10 compliance: the public surface is entirely
 * pure-virtual with no template parameters. The concrete stateful base
 * lives on @ref AbstractMessageBus and the concrete final
 * implementation on @ref SystemMessageBus; users never name those types
 * directly on hot paths.
 */
class IMessageBus
{
  public:
    virtual ~IMessageBus() = default;

    // ------ Identity ------

    /**
     * @brief Returns the stable @ref BusId assigned by the factory.
     *
     * The system bus reports @c value @c == @c 1. Additional buses
     * created through @ref createMessageBus take increasing values.
     */
    [[nodiscard]] virtual BusId id() const noexcept = 0;

    /**
     * @brief Returns the configuration this bus was built with.
     *
     * The reference is valid for the bus's lifetime. Fields are
     * immutable after construction; callers inspect them for facade
     * wiring and for diagnostics.
     */
    [[nodiscard]] virtual const BusConfig &config() const noexcept = 0;

    // ------ Registration ------

    /**
     * @brief Registers @p target with this bus and hands the target the
     *        matching @ref ConnectionToken through
     *        @ref AbstractMessageTarget::acceptConnection.
     *
     * A null @p target returns @ref Result::Code::InvalidMessageTarget
     * without mutating the registry. A call after @ref shutdown returns
     * @ref Result::Code::Error without mutating the registry. The bus
     * is strongly exception-safe: on a failure thrown from
     * @c allocateSlot, @c std::make_unique, or
     * @ref AbstractMessageTarget::acceptConnection, the registry ends
     * byte-identical to the state before the call.
     */
    [[nodiscard]] virtual Result registerTarget(AbstractMessageTarget *target) = 0;

    // ------ Publish / subscribe ------

    /**
     * @brief Enqueues @p message for dispatch.
     *
     * The bus validates @p message (non-null, kind and route mode in
     * the closed enums, target alive when non-null), applies the
     * @ref BackpressurePolicy from @ref BusConfig, and either enqueues
     * the envelope for the dispatch worker or -- under
     * @ref ThreadingPolicy::InlineOnly -- dispatches synchronously on
     * the caller's thread.
     *
     * Returns a success @ref Result when the message was accepted.
     * Returns @ref Result::Code::Error when the bus is shut down, when
     * validation fails, or when the @ref BackpressurePolicy::Error path
     * is hit while the queue is full.
     */
    [[nodiscard]] virtual Result post(std::unique_ptr<IMessage> message) = 0;

    /**
     * @brief Installs @p subscriber under @p filter and returns the
     *        RAII token that owns the slot.
     *
     * A null @p subscriber or an invalid @ref MessageKind in @p filter
     * returns an inert token whose @ref ISubscriptionToken::active
     * always reports @c false. On success, the returned token keeps the
     * slot alive until it is dropped or its
     * @ref ISubscriptionToken::cancel is called.
     */
    [[nodiscard]] virtual std::unique_ptr<ISubscriptionToken>
        subscribe(const MessageFilter &filter, ISubscriber *subscriber) = 0;

    // ------ Lifecycle ------

    /**
     * @brief Drains in-flight dispatch, cancels every subscription,
     *        flips the control block to dead, and returns.
     *
     * Idempotent: a second call is a no-op. After @ref shutdown returns,
     * @ref post fails fast and @ref subscribe returns an inert token.
     * Outstanding @ref ConnectionToken destructors observe the dead
     * state through their @c std::weak_ptr<IBusControlBlock> and skip
     * registry cleanup.
     */
    virtual Result shutdown() = 0;

    IMessageBus(const IMessageBus &)            = delete;
    IMessageBus &operator=(const IMessageBus &) = delete;
    IMessageBus(IMessageBus &&)                 = delete;
    IMessageBus &operator=(IMessageBus &&)      = delete;

  protected:
    IMessageBus() = default;
};

} // namespace vigine::messaging

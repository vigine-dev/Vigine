#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

#include "vigine/graph/abstractgraph.h"  // INV-11 EXEMPTION: AbstractMessageBus inherits graph substrate for internal routing
#include "vigine/messaging/busconfig.h"
#include "vigine/messaging/busid.h"
#include "vigine/messaging/connectionid.h"
#include "vigine/messaging/ibuscontrolblock.h"
#include "vigine/messaging/imessage.h"
#include "vigine/messaging/imessagebus.h"
#include "vigine/messaging/isubscriber.h"
#include "vigine/messaging/isubscriptiontoken.h"
#include "vigine/messaging/messagefilter.h"
#include "vigine/messaging/subscriptionslot.h"
#include "vigine/result.h"

namespace vigine::core::threading
{
class IThreadManager;
} // namespace vigine::core::threading

namespace vigine::messaging
{
class AbstractMessageTarget;
class DefaultBusControlBlock;

/**
 * @brief Stateful abstract base that carries every piece of bus state
 *        every concrete message bus reuses.
 *
 * @ref AbstractMessageBus is level 4 of the five-layer wrapper recipe
 * (see @c theory_wrapper_creation_recipe.md). It inherits
 * @ref IMessageBus @c public so the bus surface sits at offset zero for
 * zero-cost up-casts, and @ref vigine::graph::AbstractGraph
 * @c protected so the graph substrate is available to wrapper code
 * without leaking into the bus's public surface.
 *
 * The class carries state, so it follows the project's @c Abstract
 * naming convention rather than the @c I pure-virtual prefix. Everything
 * is @c private: the control block, the subscription registry, the
 * queue, the dispatch mutex, the shutdown flag, and the handle to the
 * injected @ref vigine::core::threading::IThreadManager. Concrete subclasses
 * extend the chain (for example @ref SystemMessageBus) to pin a specific
 * @ref BusConfig shape; they do not override behaviour.
 *
 * Ownership and lifetime:
 *   - The bus owns a @c std::shared_ptr<IBusControlBlock>. The control
 *     block carries BOTH the target (connection) registry and the
 *     subscription registry. Every @ref ConnectionToken handed to a
 *     registered @ref AbstractMessageTarget holds a
 *     @c std::weak_ptr to the same block; every
 *     @ref SubscriptionToken returned by @ref subscribe holds the same
 *     kind of weak reference. When the bus is destroyed first, the
 *     control block is marked dead and every outstanding token becomes
 *     a safe no-op on destruction. When a subscriber or target is
 *     destroyed first, its token drains its slot through the control
 *     block so the bus's registries never see a dangling pointer.
 *   - The injected @ref vigine::core::threading::IThreadManager is referenced
 *     by pointer; the engine guarantees the manager outlives every bus
 *     it hands a reference to.
 *
 * Thread-safety: @ref post serialises against the queue mutex; the
 * dispatch worker drains the queue under the same mutex. Both
 * registries live inside the control block, which owns its own
 * @c std::shared_mutex per registry -- dispatch takes a shared lock for
 * the duration of its snapshot; writers (register / unregister) take an
 * exclusive lock for the entry update.
 *
 * Exception policy: subscriber exceptions are isolated at the dispatch
 * boundary and reported as @ref DispatchResult::Handled so the registry
 * keeps walking. @c bad_alloc on @c post propagates to the caller; the
 * bus's own state is untouched on that path.
 */
class AbstractMessageBus
    : public IMessageBus
    , protected vigine::graph::AbstractGraph  // INV-11 EXEMPTION: routing substrate is an internal implementation detail
{
  public:
    ~AbstractMessageBus() override;

    // ------ IMessageBus ------

    [[nodiscard]] BusId               id() const noexcept override;
    [[nodiscard]] const BusConfig    &config() const noexcept override;

    [[nodiscard]] Result registerTarget(AbstractMessageTarget *target) override;
    [[nodiscard]] Result post(std::unique_ptr<IMessage> message) override;
    [[nodiscard]] std::unique_ptr<ISubscriptionToken>
        subscribe(const MessageFilter &filter, ISubscriber *subscriber) override;
    Result shutdown() override;

  protected:
    /**
     * @brief Builds the base with the supplied config and injected
     *        thread manager.
     *
     * The constructor allocates the control block eagerly so every
     * @ref registerTarget call after construction has a live block to
     * point at. The @p threadManager reference is retained for the
     * lifetime of the bus; the caller (typically the engine) guarantees
     * the manager outlives every bus it owns.
     */
    AbstractMessageBus(BusConfig config, vigine::core::threading::IThreadManager &threadManager);

    // Forwarded to subclasses so concrete constructors can inspect the
    // chosen config before falling through to user code (for example
    // @ref SystemMessageBus pins the id to the system reserved value).
    [[nodiscard]] BusConfig       &mutableConfig() noexcept { return _config; }
    [[nodiscard]] const BusConfig &immutableConfig() const noexcept { return _config; }

  private:
    // ------ Internal types ------
    //
    // `SlotState` and `SubscriptionSlot` live in
    // `vigine/messaging/subscriptionslot.h` — they are shared between
    // the bus (which builds them inside `subscribe()` before handing them
    // to the control block) and the control block (which owns the
    // registry and hands snapshot copies back to dispatch). Their doc
    // comments sit next to the definitions in that header.

    /// @brief Queue entry. Owns the envelope plus the scheduled-for
    ///        timestamp so that the worker can requeue delayed traffic
    ///        without parsing the message twice.
    struct QueuedMessage
    {
        std::unique_ptr<IMessage>                        message;
        std::chrono::steady_clock::time_point            deadline;
    };

    /// @brief Concrete subscription token handed back to callers of
    ///        @ref subscribe.
    ///
    /// The token carries a @c std::weak_ptr to the bus's
    /// @ref IBusControlBlock and the serial id that addresses its slot
    /// inside the block's subscription registry. The raw bus pointer
    /// the token used to hold has moved out entirely: the bus's
    /// lifetime is no longer observed by the token, only the control
    /// block's is. A cancel (explicit or through the destructor) locks
    /// the weak_ptr; if the lock fails or @ref IBusControlBlock::isAlive
    /// reports @c false, the cancel is a safe no-op. Otherwise the
    /// token asks the block to unregister its slot, which drains every
    /// in-flight dispatch on that slot before returning.
    ///
    /// This is the exact pattern @ref ConnectionToken uses for target
    /// registrations; the symmetry is intentional.
    class SubscriptionToken final : public ISubscriptionToken
    {
      public:
        SubscriptionToken(std::weak_ptr<IBusControlBlock> control,
                          std::uint64_t                   serial) noexcept;
        ~SubscriptionToken() override;

        void               cancel() noexcept override;
        [[nodiscard]] bool active() const noexcept override;

        SubscriptionToken(const SubscriptionToken &)            = delete;
        SubscriptionToken &operator=(const SubscriptionToken &) = delete;
        SubscriptionToken(SubscriptionToken &&)                 = delete;
        SubscriptionToken &operator=(SubscriptionToken &&)      = delete;

      private:
        std::weak_ptr<IBusControlBlock> _control;
        std::uint64_t                   _serial;
        std::atomic<bool>               _cancelled{false};
    };

    friend class SubscriptionToken;

    // ------ Dispatch helpers (implemented across the routing TUs) ------

    /// @brief Drains one message, selecting the routing algorithm.
    void dispatchOne(const IMessage &message);

    /// @brief First-matching subscriber wins; early-exit DFS over the
    ///        registry snapshot.
    void dispatchFirstMatch(const IMessage                    &message,
                            const std::vector<SubscriptionSlot> &snapshot);
    /// @brief One-level fan-out to every matching subscriber.
    void dispatchFanOut(const IMessage                    &message,
                        const std::vector<SubscriptionSlot> &snapshot);
    /// @brief Linear traversal until @ref DispatchResult::Handled or
    ///        @ref DispatchResult::Stop.
    void dispatchChain(const IMessage                    &message,
                       const std::vector<SubscriptionSlot> &snapshot);
    /// @brief Parent-chain traversal; falls back to @c FirstMatch when
    ///        the target has no parent.
    void dispatchBubble(const IMessage                    &message,
                        const std::vector<SubscriptionSlot> &snapshot);
    /// @brief Every subscriber receives the message regardless of the
    ///        filter's @c target field.
    void dispatchBroadcast(const IMessage                    &message,
                           const std::vector<SubscriptionSlot> &snapshot);

    // ------ Registry helpers ------

    /// @brief Tests whether a slot matches a message's routing and
    ///        payload tags. Target-match behaviour varies per route mode
    ///        and is handled by the dispatch methods directly.
    [[nodiscard]] static bool matches(const SubscriptionSlot &slot,
                                      const IMessage         &message) noexcept;

    /// @brief Deliver one message to one subscriber, isolating
    ///        exceptions at the dispatch boundary.
    ///
    /// Takes the slot (not just the `ISubscriber`) so the per-slot
    /// `deliverMutex` can serialise concurrent dispatches into the
    /// same subscriber. Matches the header contract that
    /// `ISubscriber::onMessage` is never invoked concurrently for
    /// the same subscriber — previously the five routing drivers
    /// called through a subscriber-only `deliver` with no
    /// serialisation, so two posting threads racing the registry
    /// could interleave on the same slot.
    [[nodiscard]] static DispatchResult deliver(const SubscriptionSlot &slot,
                                                const IMessage         &message) noexcept;

    /// @brief Builds a snapshot of the registry by delegating to the
    ///        control block. Kept as a thin wrapper so the dispatch
    ///        driver does not have to know the subscription registry
    ///        lives one level down.
    [[nodiscard]] std::vector<SubscriptionSlot> snapshotRegistry() const;

    /// @brief Drains the queue. Returns when the queue is empty and
    ///        either the shutdown flag is set or the caller requested a
    ///        single drain pass.
    void drainQueue(bool untilShutdown);

    // ------ State ------

    BusConfig                                      _config;
    vigine::core::threading::IThreadManager             *_threadManager;
    // The control block owns BOTH registries: the connection (target)
    // registry AND the subscription registry. The bus keeps this
    // shared_ptr alive for the lifetime of the bus; every
    // `ConnectionToken` and every `SubscriptionToken` holds a
    // `weak_ptr` to the same block so that a bus destroyed before its
    // tokens is observed as a dead block instead of a dangling bus
    // pointer.
    std::shared_ptr<DefaultBusControlBlock>        _control;

    mutable std::mutex                             _queueMutex;
    std::condition_variable                        _queueCv;
    std::deque<QueuedMessage>                      _queue;

    std::atomic<bool>                              _shutdown{false};
};

} // namespace vigine::messaging

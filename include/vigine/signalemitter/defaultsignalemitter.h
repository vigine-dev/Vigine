#pragma once

#include <memory>

#include "vigine/messaging/busconfig.h"
#include "vigine/signalemitter/abstractsignalemitter.h"

namespace vigine::core::threading
{
class IThreadManager;
} // namespace vigine::core::threading

namespace vigine::signalemitter
{

/**
 * @brief Concrete final signal-emitter facade that closes the five-layer
 *        wrapper recipe for the signal pattern.
 *
 * @ref DefaultSignalEmitter is Level-5 of the five-layer wrapper recipe.
 * It extends @ref AbstractSignalEmitter with no additional storage or
 * behaviour of its own; its role is to seal the chain via @c final and
 * expose constructors that either pick a suitable default
 * @ref vigine::messaging::BusConfig for the internal bus or accept one
 * provided by the caller.
 *
 * Callers obtain instances exclusively through
 * @ref createSignalEmitter — they never construct this type by name.
 *
 * Invariants:
 *   - @c final: no further subclassing allowed.
 *   - All state lives in @ref AbstractSignalEmitter and
 *     @ref vigine::messaging::AbstractMessageBus; this class adds none.
 */
class DefaultSignalEmitter final : public AbstractSignalEmitter
{
  public:
    /**
     * @brief Constructs the emitter using an inline-only bus backed by
     *        @p threadManager.
     *
     * The inline-only threading policy keeps the signal dispatch
     * synchronous on the caller's thread, which is the historical
     * default and the shape exercised by the facade contract case.
     */
    explicit DefaultSignalEmitter(vigine::core::threading::IThreadManager &threadManager);

    /**
     * @brief Constructs the emitter using the caller-supplied @p config
     *        for the internal bus, backed by @p threadManager.
     *
     * Use this overload to opt into a non-inline threading policy — for
     * example the config returned by @ref sharedBusConfig to put dispatch
     * on the thread manager's shared worker pool.
     */
    DefaultSignalEmitter(vigine::core::threading::IThreadManager &threadManager,
                         vigine::messaging::BusConfig       config);

    ~DefaultSignalEmitter() override = default;

    // ISignalEmitter
    [[nodiscard]] vigine::Result
        emit(std::unique_ptr<ISignalPayload> payload) override;

    [[nodiscard]] vigine::Result
        emitTo(const vigine::messaging::AbstractMessageTarget *target,
               std::unique_ptr<ISignalPayload>                 payload) override;

    [[nodiscard]] std::unique_ptr<vigine::messaging::ISubscriptionToken>
        subscribeSignal(vigine::messaging::MessageFilter  filter,
                        vigine::messaging::ISubscriber   *subscriber) override;

    DefaultSignalEmitter(const DefaultSignalEmitter &)            = delete;
    DefaultSignalEmitter &operator=(const DefaultSignalEmitter &) = delete;
    DefaultSignalEmitter(DefaultSignalEmitter &&)                  = delete;
    DefaultSignalEmitter &operator=(DefaultSignalEmitter &&)       = delete;
};

/**
 * @brief Returns a @ref vigine::messaging::BusConfig suitable for a
 *        shared-pool signal bus.
 *
 * Mirrors the internal default used by @ref DefaultSignalEmitter's
 * no-config constructor, but with
 * @ref vigine::messaging::ThreadingPolicy::Shared and a distinct @c name
 * field so diagnostics can tell the two apart.
 *
 * Current threading reality: @c ThreadingPolicy::Shared is intended to
 * dispatch the signal on a bus worker thread, but today
 * @ref vigine::messaging::AbstractMessageBus::post drains @c Shared
 * queues on the posting thread (the dedicated worker pump is deferred
 * to a later lifecycle change). A caller that needs a guaranteed
 * thread hop should pair this config with a deliberate
 * @ref vigine::core::threading::IThreadManager::schedule on the producer
 * side, or use the non-@c Any @ref core::threading::ThreadAffinity path on
 * @ref TaskFlow::signal which wraps the subscription in an adapter
 * that re-posts onto the thread manager.
 */
[[nodiscard]] vigine::messaging::BusConfig sharedBusConfig() noexcept;

/**
 * @brief Factory function — the sole entry point for creating a
 *        signal-emitter facade with the default (inline-only) bus config.
 *
 * Returns a @c std::unique_ptr so the caller owns the facade exclusively
 * (FF-1). The internal bus is backed by @p threadManager; the manager
 * must outlive the returned emitter.
 */
[[nodiscard]] std::unique_ptr<ISignalEmitter>
    createSignalEmitter(vigine::core::threading::IThreadManager &threadManager);

/**
 * @brief Factory overload that creates a signal-emitter facade with a
 *        caller-chosen @p config for the internal bus.
 *
 * Typical use is to pass @ref sharedBusConfig for an async dispatch path
 * while still keeping the facade as the only public handle on the
 * emitter. @p threadManager must outlive the returned emitter.
 */
[[nodiscard]] std::unique_ptr<ISignalEmitter>
    createSignalEmitter(vigine::core::threading::IThreadManager &threadManager,
                        vigine::messaging::BusConfig       config);

} // namespace vigine::signalemitter

#pragma once

#include <memory>

#include "vigine/api/messaging/busconfig.h"
#include "vigine/api/messaging/abstractsignalemitter.h"

namespace vigine::core::threading
{
class IThreadManager;
} // namespace vigine::core::threading

namespace vigine::payload
{
class IPayloadRegistry;
} // namespace vigine::payload

namespace vigine::messaging
{

/**
 * @brief Concrete final signal-emitter facade that closes the five-layer
 *        wrapper recipe for the signal pattern.
 *
 * @ref SignalEmitter is Level-5 of the five-layer wrapper recipe.
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
class SignalEmitter final : public AbstractSignalEmitter
{
  public:
    /**
     * @brief Constructs the emitter using an inline-only bus backed by
     *        @p threadManager.
     *
     * The inline-only threading policy keeps the signal dispatch
     * synchronous on the caller's thread, which is the historical
     * default and the shape exercised by the facade contract case.
     *
     * No payload-id validation: callers that want
     * @ref vigine::payload::IPayloadRegistry-backed validation use the
     * registry-aware constructor below.
     */
    explicit SignalEmitter(vigine::core::threading::IThreadManager &threadManager);

    /**
     * @brief Constructs the emitter using the caller-supplied @p config
     *        for the internal bus, backed by @p threadManager.
     *
     * Use this overload to opt into a non-inline threading policy — for
     * example the config returned by @ref sharedBusConfig to put dispatch
     * on the thread manager's shared worker pool.
     */
    SignalEmitter(vigine::core::threading::IThreadManager &threadManager,
                         vigine::messaging::BusConfig       config);

    /**
     * @brief Constructs the emitter with payload-id validation backed
     *        by @p registry.
     *
     * Every @ref emit / @ref emitTo call resolves the payload's
     * @ref vigine::payload::PayloadTypeId against @p registry before
     * posting; an unregistered id surfaces as an error
     * @ref vigine::Result with a diagnostic message instead of a
     * silently-dropped message. @p registry must outlive the emitter
     * (the engine context guarantees this through member declaration
     * order).
     */
    SignalEmitter(vigine::core::threading::IThreadManager &threadManager,
                  vigine::messaging::BusConfig             config,
                  vigine::payload::IPayloadRegistry       &registry);

    ~SignalEmitter() override = default;

    // ISignalEmitter
    [[nodiscard]] vigine::Result
        emit(std::unique_ptr<ISignalPayload> payload) override;

    [[nodiscard]] vigine::Result
        emitTo(const vigine::messaging::AbstractMessageTarget *target,
               std::unique_ptr<ISignalPayload>                 payload) override;

    [[nodiscard]] std::unique_ptr<vigine::messaging::ISubscriptionToken>
        subscribeSignal(vigine::messaging::MessageFilter  filter,
                        vigine::messaging::ISubscriber   *subscriber) override;

    SignalEmitter(const SignalEmitter &)            = delete;
    SignalEmitter &operator=(const SignalEmitter &) = delete;
    SignalEmitter(SignalEmitter &&)                  = delete;
    SignalEmitter &operator=(SignalEmitter &&)       = delete;

  private:
    /**
     * @brief Optional non-owning reference to the payload-id registry.
     *
     * Set by the validation-aware constructor; remains @c nullptr
     * for the legacy two no-validation constructors. When non-null,
     * @ref emit / @ref emitTo consult the registry before posting and
     * fail-fast on an unregistered id.
     */
    vigine::payload::IPayloadRegistry *_payloadRegistry{nullptr};
};

/**
 * @brief Returns a @ref vigine::messaging::BusConfig suitable for a
 *        shared-pool signal bus.
 *
 * Mirrors the internal default used by @ref SignalEmitter's
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

/**
 * @brief Factory that builds a validation-aware emitter — every
 *        @c emit / @c emitTo consults @p registry and rejects an
 *        unregistered @ref vigine::payload::PayloadTypeId.
 *
 * @p registry must outlive the returned emitter; the engine context
 * guarantees this through member declaration order. @p threadManager
 * must outlive the registry.
 */
[[nodiscard]] std::unique_ptr<ISignalEmitter>
    createSignalEmitter(vigine::core::threading::IThreadManager &threadManager,
                        vigine::messaging::BusConfig             config,
                        vigine::payload::IPayloadRegistry       &registry);

} // namespace vigine::messaging

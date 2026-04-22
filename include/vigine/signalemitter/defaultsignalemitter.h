#pragma once

#include <memory>

#include "vigine/signalemitter/abstractsignalemitter.h"

namespace vigine::threading
{
class IThreadManager;
} // namespace vigine::threading

namespace vigine::signalemitter
{

/**
 * @brief Concrete final signal-emitter facade that closes the five-layer
 *        wrapper recipe for the signal pattern.
 *
 * @ref DefaultSignalEmitter is Level-5 of the five-layer wrapper recipe.
 * It extends @ref AbstractSignalEmitter with no additional storage or
 * behaviour of its own; its role is to seal the chain via @c final and
 * expose a constructor that picks a suitable default
 * @ref vigine::messaging::BusConfig for the internal bus.
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
     * synchronous on the caller's thread; facade clients that need a
     * dedicated dispatch thread create the bus externally and supply a
     * matching config via the protected
     * @ref AbstractSignalEmitter constructor.
     */
    explicit DefaultSignalEmitter(vigine::threading::IThreadManager &threadManager);

    ~DefaultSignalEmitter() override = default;

    // ISignalEmitter
    [[nodiscard]] vigine::Result
        emit(std::unique_ptr<ISignalPayload> payload) override;

    [[nodiscard]] vigine::Result
        emitTo(const vigine::messaging::AbstractMessageTarget *target,
               std::unique_ptr<ISignalPayload>                 payload) override;

    DefaultSignalEmitter(const DefaultSignalEmitter &)            = delete;
    DefaultSignalEmitter &operator=(const DefaultSignalEmitter &) = delete;
    DefaultSignalEmitter(DefaultSignalEmitter &&)                  = delete;
    DefaultSignalEmitter &operator=(DefaultSignalEmitter &&)       = delete;
};

/**
 * @brief Factory function — the sole entry point for creating a
 *        signal-emitter facade.
 *
 * Returns a @c std::unique_ptr so the caller owns the facade exclusively
 * (FF-1). The internal bus is backed by @p threadManager; the manager
 * must outlive the returned emitter.
 */
[[nodiscard]] std::unique_ptr<ISignalEmitter>
    createSignalEmitter(vigine::threading::IThreadManager &threadManager);

} // namespace vigine::signalemitter

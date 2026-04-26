#pragma once

#include <memory>

#include "vigine/api/messaging/imessagepayload.h"

namespace vigine::messaging
{

/**
 * @brief Pure-virtual base for every signal payload carried through the
 *        @ref ISignalEmitter facade.
 *
 * @ref ISignalPayload extends @ref vigine::messaging::IMessagePayload with
 * a single hook — @ref clone — that every concrete payload must provide.
 * Otherwise the surface is the same: the facade and its callers refer to
 * a distinct type that documents "this is a signal payload", while the
 * bus infrastructure keeps routing by the underlying
 * @ref vigine::payload::PayloadTypeId returned by @ref typeId.
 *
 * Concrete signal payloads derive from this class, provide a unique,
 * registered @ref vigine::payload::PayloadTypeId, and store their fields
 * as @c const members set at construction time. Immutability is required
 * by the messaging contract: the bus may deliver the same payload
 * pointer to multiple subscribers without copying.
 *
 * Registration requirement: the engine does NOT auto-register
 * user-defined payload types. Each application is responsible for
 * obtaining its @ref vigine::payload::PayloadTypeId from
 * @ref vigine::payload::IPayloadRegistry — typically through
 * @c allocateId / @c allocateRange on the registry exposed via
 * @c IContext::payloadRegistry — before any @c TaskFlow::signal
 * subscription or @c ISignalEmitter::emit call that references it.
 * An unregistered id is rejected at emit time and never matches a
 * subscription at dispatch time. Engine-bundled payload types arrive
 * pre-registered; only application-defined subclasses need this glue.
 *
 * Naming: @c ISignalPayload follows INV-10 — @c I prefix for a pure-virtual
 * interface with no state.
 */
class ISignalPayload : public vigine::messaging::IMessagePayload
{
  public:
    ~ISignalPayload() override = default;

    // typeId() is inherited from IMessagePayload and remains pure-virtual.
    // Concrete payloads must return a stable, registered PayloadTypeId.

    /**
     * @brief Returns a deep-owned copy of this payload.
     *
     * The non-inline @c TaskFlow::signal path schedules delivery onto a
     * worker thread. By the time the worker runs, the originating
     * @ref vigine::messaging::IMessage (and the payload pointer it
     * exposed) is long gone — the emitter's bus has unwound its
     * dispatch frame and destroyed the envelope. The scheduled
     * delivery therefore has to own the payload outright. Every
     * concrete implementation forwards its @c const fields into a
     * fresh instance. Immutability of the source is preserved: the
     * clone is independent and can outlive the original payload
     * pointer safely.
     */
    [[nodiscard]] virtual std::unique_ptr<ISignalPayload> clone() const = 0;

  protected:
    ISignalPayload() = default;
};

} // namespace vigine::messaging

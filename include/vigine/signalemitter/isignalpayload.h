#pragma once

#include "vigine/messaging/imessagepayload.h"

namespace vigine::signalemitter
{

/**
 * @brief Pure-virtual base for every signal payload carried through the
 *        @ref ISignalEmitter facade.
 *
 * @ref ISignalPayload extends @ref vigine::messaging::IMessagePayload with
 * no additional surface; it exists as a named base so that the facade and
 * its callers can refer to a distinct type that documents "this is a
 * signal payload", while the bus infrastructure keeps routing by the
 * underlying @ref vigine::payload::PayloadTypeId returned by @ref typeId.
 *
 * Concrete signal payloads derive from this class, provide a unique
 * @ref vigine::payload::PayloadTypeId, and store their fields as @c const
 * members set at construction time. Immutability is required by the
 * messaging contract: the bus may deliver the same payload pointer to
 * multiple subscribers without copying.
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

  protected:
    ISignalPayload() = default;
};

} // namespace vigine::signalemitter

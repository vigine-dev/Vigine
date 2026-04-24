#pragma once

/**
 * @file stateinvalidatedpayload.h
 * @brief Signal payload announcing that a state transition has
 *        invalidated every @ref vigine::engine::IEngineToken bound to
 *        the vacated state.
 *
 * The state machine emits this payload on the engine-wide signal bus
 * as part of its transition post-back, immediately after flipping the
 * alive flag on the tokens that belonged to the vacated state. Tasks
 * that subscribed to expiration through the signal emitter rather
 * than through @ref vigine::engine::IEngineToken::subscribeExpiration
 * observe the payload and perform their own cleanup. The concrete
 * wiring into @c AbstractStateMachine lands in a follow-up issue; this
 * header ships the payload contract only.
 *
 * Payload discipline follows the
 * @ref vigine::signalemitter::ISignalPayload contract:
 *   - Fields are @c const and set at construction time. The bus may
 *     deliver the same payload pointer to multiple subscribers
 *     without copying, so observable state never changes after
 *     publish.
 *   - @ref typeId returns a stable
 *     @ref vigine::payload::PayloadTypeId value registered with the
 *     engine's payload registry at initialisation time.
 *   - @ref clone returns an independent deep-owned copy for
 *     non-inline dispatch paths (see @c TaskFlow::signal).
 */

#include <memory>

#include "vigine/payload/payloadtypeid.h"
#include "vigine/signalemitter/isignalpayload.h"
#include "vigine/statemachine/stateid.h"

/**
 * @brief Payload identifier for @ref vigine::messaging::payload::StateInvalidatedPayload.
 *
 * User-range value; registered by the engine's payload registry before
 * the state machine emits the first invalidation signal. The 0x203xx
 * block is reserved for state-machine lifecycle payloads so future
 * additions (e.g. state-entered, state-exited) extend contiguously
 * without colliding with existing signal-emitter or window-example
 * payloads.
 */
inline constexpr vigine::payload::PayloadTypeId kStateInvalidatedPayloadTypeId{0x20301u};

namespace vigine::messaging::payload
{

/**
 * @brief Immutable payload describing the state whose tokens have just
 *        been invalidated.
 *
 * Carries a copy of the @ref vigine::statemachine::StateId the state
 * machine has just left. Subscribers compare it against their own
 * bound state (if any) to decide whether the signal applies to them.
 */
class StateInvalidatedPayload final : public vigine::signalemitter::ISignalPayload
{
  public:
    /**
     * @brief Stamps the payload with the @p invalidatedState handle.
     */
    explicit StateInvalidatedPayload(
        vigine::statemachine::StateId invalidatedState) noexcept
        : _invalidatedState(invalidatedState)
    {
    }

    ~StateInvalidatedPayload() override = default;

    // ------ ISignalPayload ------

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        return kStateInvalidatedPayloadTypeId;
    }

    [[nodiscard]] std::unique_ptr<vigine::signalemitter::ISignalPayload>
        clone() const override
    {
        return std::make_unique<StateInvalidatedPayload>(_invalidatedState);
    }

    // ------ Payload accessors ------

    /**
     * @brief Returns the @ref vigine::statemachine::StateId the FSM
     *        has just left.
     *
     * Stable for the payload's lifetime. Callers compare the value
     * against their own bound state (e.g. their token's
     * @c boundState()) and skip the signal when it does not match.
     */
    [[nodiscard]] vigine::statemachine::StateId invalidatedState() const noexcept
    {
        return _invalidatedState;
    }

    StateInvalidatedPayload(const StateInvalidatedPayload &)            = delete;
    StateInvalidatedPayload &operator=(const StateInvalidatedPayload &) = delete;
    StateInvalidatedPayload(StateInvalidatedPayload &&)                 = delete;
    StateInvalidatedPayload &operator=(StateInvalidatedPayload &&)      = delete;

  private:
    const vigine::statemachine::StateId _invalidatedState;
};

} // namespace vigine::messaging::payload

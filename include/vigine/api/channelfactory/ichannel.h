#pragma once

#include <memory>

#include "vigine/api/messaging/imessagepayload.h"
#include "vigine/api/messaging/payload/payloadtypeid.h"
#include "vigine/result.h"

namespace vigine::channelfactory
{

/**
 * @brief Pure-virtual per-channel operations for a CSP-style bounded channel.
 *
 * @ref IChannel is the per-handle interface vended by @ref IChannelFactory.
 * One factory produces many channels; each channel is a typed point-to-point
 * pipe. Type safety is achieved through @ref vigine::payload::PayloadTypeId:
 * every @ref send call is validated against the expected type id the channel
 * was created with, and a mismatch returns @ref vigine::Result::Code::Error.
 *
 * Ownership:
 *   - @ref send takes unique ownership of the payload on success; on failure
 *     the caller retains ownership.
 *   - @ref receive and @ref tryReceive transfer ownership of a dequeued
 *     payload to the caller.
 *
 * Thread-safety: all entry points are safe to call from any thread
 * concurrently.
 *
 * Lifecycle:
 *   - @ref close flips the channel closed; further @ref send calls return an
 *     error; @ref receive and @ref tryReceive drain any remaining payloads
 *     then report closure.
 *   - Idempotent: the second and subsequent @ref close calls are no-ops.
 *
 * Invariants:
 *   - INV-1: no template parameters in the public surface.
 *   - INV-10: @c I prefix for a pure-virtual interface (no state).
 *   - INV-11: no graph types appear in this header.
 */
class IChannel
{
  public:
    virtual ~IChannel() = default;

    /**
     * @brief Sends @p payload, blocking up to @p timeoutMs milliseconds when
     *        the channel is full (Bounded) or until the channel is closed.
     *
     * The @p payload type id must match the expected id the channel was
     * created with; a mismatch returns @ref vigine::Result::Code::Error and
     * the caller retains ownership of @p payload.
     *
     * Returns a success @ref vigine::Result and transfers ownership when the
     * payload was enqueued. Returns an error when the channel is closed, the
     * timeout elapses (Bounded channels), or the payload type id does not
     * match.
     */
    [[nodiscard]] virtual vigine::Result
        send(std::unique_ptr<vigine::messaging::IMessagePayload> payload,
             int timeoutMs = -1) = 0;

    /**
     * @brief Attempts to send @p payload without blocking.
     *
     * Returns @c true and transfers ownership on success. Returns @c false
     * without mutating @p payload when the channel is full, closed, or the
     * type id does not match.
     */
    [[nodiscard]] virtual bool
        trySend(std::unique_ptr<vigine::messaging::IMessagePayload> &payload) = 0;

    /**
     * @brief Receives a payload, blocking up to @p timeoutMs milliseconds
     *        when the channel is empty.
     *
     * On success @p out receives ownership of the dequeued payload and the
     * returned @ref vigine::Result is successful. When @p timeoutMs elapses
     * before a payload becomes available, or when the channel is both closed
     * and empty, the returned @ref vigine::Result is an error and @p out is
     * left default-constructed.
     *
     * A value of -1 for @p timeoutMs means "block indefinitely".
     */
    [[nodiscard]] virtual vigine::Result
        receive(std::unique_ptr<vigine::messaging::IMessagePayload> &out,
                int timeoutMs = -1) = 0;

    /**
     * @brief Attempts to receive a payload without blocking.
     *
     * Returns @c true and transfers ownership into @p out when a payload is
     * immediately available. Returns @c false without mutating @p out when
     * the channel is empty (whether or not it is closed). A closed-and-empty
     * channel returns @c false.
     */
    [[nodiscard]] virtual bool
        tryReceive(std::unique_ptr<vigine::messaging::IMessagePayload> &out) = 0;

    /**
     * @brief Closes the channel.
     *
     * Idempotent. After close, @ref send fails immediately; @ref receive and
     * @ref tryReceive drain remaining payloads then signal closure. Threads
     * blocked in @ref send or @ref receive wake up with an error
     * @ref vigine::Result.
     */
    virtual void close() = 0;

    /**
     * @brief Returns @c true if @ref close has been called at least once.
     *
     * Diagnostic accessor; the value may change between the call and the
     * next operation on the channel.
     */
    [[nodiscard]] virtual bool isClosed() const = 0;

    /**
     * @brief Returns the number of payloads currently queued.
     *
     * Diagnostic accessor; the value may change between the call and the
     * next operation on the channel.
     */
    [[nodiscard]] virtual std::size_t size() const = 0;

    IChannel(const IChannel &)            = delete;
    IChannel &operator=(const IChannel &) = delete;
    IChannel(IChannel &&)                 = delete;
    IChannel &operator=(IChannel &&)      = delete;

  protected:
    IChannel() = default;
};

} // namespace vigine::channelfactory

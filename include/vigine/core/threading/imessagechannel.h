#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "vigine/payload/payloadtypeid.h"
#include "vigine/result.h"

namespace vigine::core::threading
{
/**
 * @brief Owning wrapper around one payload byte buffer carried through
 *        @ref IMessageChannel.
 *
 * @ref Message is a deliberately narrow value type. It pairs a
 * @ref vigine::payload::PayloadTypeId (the key subscribers look up to
 * decide how to decode the bytes) with a heap buffer and a byte count.
 * The buffer is owned through a @c std::unique_ptr so that @ref Message
 * is move-only and does not leak when callers discard it mid-transfer.
 *
 * The type is intentionally non-templated. A templated channel would
 * violate INV-1 (no templates in public threading headers); a typed
 * wrapper-over-bytes keeps the ABI stable and lets subscribers cast by
 * payload id at the decode site.
 *
 * Typical producer pattern:
 * @code
 *   auto buffer = std::make_unique<std::byte[]>(payload.size());
 *   std::memcpy(buffer.get(), payload.data(), payload.size());
 *   Message msg{PayloadTypeId{kMyPayloadId}, std::move(buffer), payload.size()};
 *   channel->send(std::move(msg), std::chrono::milliseconds::max());
 * @endcode
 */
// ENCAP EXEMPT: pure value aggregate
struct Message
{
    /// Payload-registry key the receiver uses to decode @ref bytes.
    vigine::payload::PayloadTypeId typeId{};

    /// Owned byte buffer carrying the serialised payload (may be null
    /// when @ref sizeBytes is zero).
    std::unique_ptr<std::byte[]> bytes;

    /// Number of valid bytes in @ref bytes.
    std::size_t sizeBytes{0};

    Message()                                 = default;
    Message(vigine::payload::PayloadTypeId id,
            std::unique_ptr<std::byte[]>   buffer,
            std::size_t                    size) noexcept
        : typeId{id}, bytes{std::move(buffer)}, sizeBytes{size}
    {
    }

    Message(const Message &)            = delete;
    Message &operator=(const Message &) = delete;
    Message(Message &&) noexcept            = default;
    Message &operator=(Message &&) noexcept = default;
};

/**
 * @brief Pure-virtual bounded FIFO channel produced by
 *        @ref IThreadManager::createMessageChannel.
 *
 * @ref IMessageChannel is the engine's cross-thread message pipe. A
 * fixed capacity is supplied at construction through the factory on
 * @ref IThreadManager. Producers call @ref send (blocking until space
 * is available) or @ref trySend (non-blocking). Consumers call
 * @ref receive (blocking) or @ref tryReceive. @ref close flips the
 * channel into a drained state: waiting senders and receivers wake up
 * with an error @ref Result, further sends fail, and further receives
 * succeed only as long as the channel still has queued messages.
 *
 * Ownership and lifetime: the channel is owned by the caller via
 * @c std::unique_ptr. Messages move through the channel — each
 * @ref send transfers ownership of the message into the channel and
 * each @ref receive transfers it back out. Destroying a channel that
 * still holds messages discards them silently; in practice callers
 * should @ref close first so waiters wake up deterministically.
 *
 * Thread-safety: every entry point is safe to call from any thread.
 * Implementations use two condition variables (one for "not full", one
 * for "not empty") to keep producer and consumer wake-ups independent.
 */
class IMessageChannel
{
  public:
    virtual ~IMessageChannel() = default;

    /**
     * @brief Sends a message, blocking up to @p timeout when the channel
     *        is full.
     *
     * `Message` is move-only, so callers invoke
     * `channel.send(std::move(msg))`. Ownership transfers into the
     * parameter before the function body runs; the original variable is
     * in a valid-but-unspecified moved-from state immediately after the
     * call returns, regardless of outcome.
     *
     * Returns a successful @ref Result when the message was queued.
     * Returns an error @ref Result when @p timeout elapses before space
     * becomes available, or when the channel is closed. The moved-from
     * `Message` on the caller side is NOT restored on failure — callers
     * that need retry semantics must reconstruct the message before
     * calling again. For the "inspect failure and retry with the same
     * payload" pattern, prefer @ref trySend, which takes a reference
     * and leaves ownership with the caller when it returns false.
     */
    [[nodiscard]] virtual Result send(Message message, std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) = 0;

    /**
     * @brief Attempts to send a message without blocking.
     *
     * Returns @c true and transfers ownership of @p message into the
     * channel when space is immediately available. Returns @c false
     * without transferring ownership when the channel is full or
     * closed.
     */
    [[nodiscard]] virtual bool trySend(Message &message) = 0;

    /**
     * @brief Receives a message, blocking up to @p timeout when the
     *        channel is empty.
     *
     * On success @p out receives ownership of the dequeued message and
     * the returned @ref Result is successful. On timeout or close with
     * no queued messages the returned @ref Result is an error and
     * @p out is left default-constructed.
     */
    [[nodiscard]] virtual Result receive(Message &out, std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) = 0;

    /**
     * @brief Attempts to receive a message without blocking.
     *
     * Returns @c true and transfers ownership into @p out when a message
     * is immediately available. Returns @c false without mutating
     * @p out when the channel is empty (whether or not it is closed).
     */
    [[nodiscard]] virtual bool tryReceive(Message &out) = 0;

    /**
     * @brief Closes the channel.
     *
     * Idempotent: the second and subsequent calls are no-ops. After
     * close, @ref send and @ref trySend fail immediately; @ref receive
     * and @ref tryReceive continue to drain any queued messages and
     * fail only once the channel is both closed and empty. Threads
     * blocked in @ref send or @ref receive wake up with an error
     * @ref Result.
     */
    virtual void close() = 0;

    /**
     * @brief Reports whether @ref close has been called.
     *
     * Diagnostic accessor. The value may change between the call and
     * the next operation on the channel.
     */
    [[nodiscard]] virtual bool isClosed() const = 0;

    /**
     * @brief Number of messages currently queued in the channel.
     *
     * Diagnostic accessor. The value may change between the call and
     * the next operation on the channel.
     */
    [[nodiscard]] virtual std::size_t size() const = 0;

    /**
     * @brief Fixed capacity of the channel, supplied at construction.
     */
    [[nodiscard]] virtual std::size_t capacity() const = 0;

    IMessageChannel(const IMessageChannel &)            = delete;
    IMessageChannel &operator=(const IMessageChannel &) = delete;
    IMessageChannel(IMessageChannel &&)                 = delete;
    IMessageChannel &operator=(IMessageChannel &&)      = delete;

  protected:
    IMessageChannel() = default;
};

} // namespace vigine::core::threading

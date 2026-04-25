#pragma once

#include <chrono>
#include <memory>
#include <optional>

#include "vigine/messaging/imessagepayload.h"

namespace vigine::requestbus
{

/**
 * @brief Pure-virtual handle returned by @ref IRequestBus::request.
 *
 * @ref IFuture is the caller's receipt for a pending request.
 * It provides a blocking wait surface; the caller suspends until the
 * matching responder posts a reply or the supplied timeout elapses.
 *
 * Type erasure: the resolved payload is an @ref IMessagePayload. The
 * caller downcasts with @c static_cast after inspecting the payload's
 * @ref vigine::payload::PayloadTypeId (INV-1 compliance -- no
 * @c IFuture<T> template form).
 *
 * Ownership:
 *   - @ref wait returns a @c std::optional<std::unique_ptr<IMessagePayload>>.
 *     @c std::nullopt means the wait timed out; a populated optional
 *     transfers ownership of the reply payload to the caller.
 *   - @ref cancel is best-effort; the future cannot revoke an
 *     in-transit message the responder has already posted.
 *
 * Thread-safety: @ref wait, @ref ready, and @ref cancel are safe to
 * call from any thread. Only one thread must call @ref wait at a time.
 *
 * Invariants:
 *   - INV-1: no template parameters in the public surface.
 *   - INV-10: @c I prefix for a pure-virtual interface (no state).
 *   - INV-11: no graph types appear in this header.
 */
class IFuture
{
  public:
    virtual ~IFuture() = default;

    /**
     * @brief Returns @c true when the reply has already arrived.
     *
     * Non-blocking. If this returns @c true, the next @ref wait call
     * returns immediately.
     */
    [[nodiscard]] virtual bool ready() const noexcept = 0;

    /**
     * @brief Blocks until a reply arrives or @p timeout elapses.
     *
     * Returns a populated @c optional carrying the reply payload on
     * success; returns @c std::nullopt when the timeout elapses with
     * no reply.
     *
     * Calling @ref wait after it has already returned a payload is
     * undefined behaviour (the payload was moved out).
     */
    [[nodiscard]] virtual std::optional<std::unique_ptr<vigine::messaging::IMessagePayload>>
        wait(std::chrono::milliseconds timeout) = 0;

    /**
     * @brief Cancels the pending request.
     *
     * Best-effort: if the reply has already been dispatched the
     * payload is discarded by the bus. After @ref cancel, a
     * subsequent @ref wait returns @c std::nullopt immediately.
     */
    virtual void cancel() = 0;

    IFuture(const IFuture &)            = delete;
    IFuture &operator=(const IFuture &) = delete;
    IFuture(IFuture &&)                 = delete;
    IFuture &operator=(IFuture &&)      = delete;

  protected:
    IFuture() = default;
};

} // namespace vigine::requestbus

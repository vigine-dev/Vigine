#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>

#include "vigine/messaging/connectionid.h"
#include "vigine/messaging/iconnectiontoken.h"

namespace vigine::messaging
{
class IBusControlBlock;

/**
 * @brief Concrete RAII token bound to a single bus-side connection slot.
 *
 * @ref ConnectionToken is the only shipped implementation of
 * @ref IConnectionToken. It carries a @c std::weak_ptr to the bus's
 * @ref IBusControlBlock so that it tolerates the bus being destroyed
 * first, plus the @ref ConnectionId that addresses its slot inside that
 * block. The destructor enforces the strong-unsubscribe barrier: the
 * slot is unregistered from the bus, and the destructor then waits until
 * every in-flight @c onMessage call that targets this token has
 * returned. That wait-until-drained step is what keeps the subscriber's
 * lifetime strictly longer than any dispatch that could still be running
 * against it.
 *
 * A token is non-copyable and non-movable: the RAII contract ties one
 * token to one slot for its entire lifetime. The @c _inFlight counter
 * and condition variable are private plumbing driven by the bus during
 * dispatch; callers never touch them directly.
 *
 * Thread-safety: construction, @ref active, and destruction are safe to
 * call from any thread. The destructor is permitted to block until
 * in-flight dispatches on this slot complete, which is the whole point
 * of the strong-unsubscribe guarantee.
 *
 * Reentrancy: a target's @c onMessage implementation must not destroy
 * the token (or the enclosing target) while the call is in flight.
 * Doing so would make the destructor wait for itself -- a self-deadlock.
 * Implementations document this as a hard precondition; runtime
 * detection is out of scope for this leaf.
 */
class ConnectionToken final : public IConnectionToken
{
  public:
    /**
     * @brief Builds a token bound to the slot addressed by @p id inside
     *        the control block reachable through @p control.
     *
     * The @c std::weak_ptr lets the token tolerate the bus being
     * destroyed before the target. @p id is stored as-is; invalid ids
     * (generation zero) produce a token whose @ref active always
     * reports @c false.
     */
    ConnectionToken(std::weak_ptr<IBusControlBlock> control, ConnectionId id) noexcept;

    /**
     * @brief Unregisters the slot (if the bus is still alive) and then
     *        waits for every in-flight dispatch on this slot to drain.
     *
     * See the class-level note on reentrancy: destroying a token from
     * inside its own dispatch causes a self-deadlock.
     */
    ~ConnectionToken() override;

    /**
     * @brief Returns @c true when both the control block is reachable
     *        and @ref IBusControlBlock::isAlive reports @c true.
     *
     * Cheap: one @c weak_ptr::lock plus one atomic load. The return
     * value is a momentary snapshot; callers racing with
     * @ref IBusControlBlock::markDead may observe the bus transitioning
     * to dead between the @c active check and a subsequent dispatch.
     */
    [[nodiscard]] bool active() const noexcept override;

    /**
     * @brief Returns the connection id addressing this token's slot.
     *
     * Stable for the lifetime of the token. Safe to call concurrently
     * with any other member function.
     */
    [[nodiscard]] ConnectionId id() const noexcept { return _id; }

    /**
     * @brief Records that a dispatch targeting this slot is about to
     *        start. Called by the bus on the dispatch hot path.
     *
     * The paired @ref endDispatch must run on exactly one thread per
     * @ref beginDispatch. Both are wait-free.
     */
    void beginDispatch() noexcept;

    /**
     * @brief Records that the matching dispatch has returned.
     *
     * When the in-flight counter transitions from @c 1 to @c 0, the
     * destructor waiter (if any) is notified.
     */
    void endDispatch() noexcept;

    ConnectionToken(const ConnectionToken &)            = delete;
    ConnectionToken &operator=(const ConnectionToken &) = delete;
    ConnectionToken(ConnectionToken &&)                 = delete;
    ConnectionToken &operator=(ConnectionToken &&)      = delete;

  private:
    std::weak_ptr<IBusControlBlock> _control;
    ConnectionId                    _id;
    std::atomic<std::uint32_t>      _inFlight{0};
    mutable std::mutex              _waitMutex;
    std::condition_variable_any     _waitCv;
};

} // namespace vigine::messaging

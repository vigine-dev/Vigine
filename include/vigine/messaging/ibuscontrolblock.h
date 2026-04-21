#pragma once

#include "vigine/messaging/connectionid.h"

namespace vigine::messaging
{
class AbstractMessageTarget;

/**
 * @brief Pure-virtual shared-heap state owned jointly by a message bus
 *        and the connection tokens it hands out.
 *
 * @ref IBusControlBlock is the safety anchor that removes the
 * destruction-ordering hazard between an @ref IMessageBus and its
 * subscribers. The bus holds a @c std::shared_ptr to the control block
 * and every token holds a @c std::weak_ptr to the same block. When the
 * bus dies first, its destructor calls @ref markDead so that every still-
 * held token becomes a safe no-op on destruction; when a target dies
 * first, the token's destructor calls @ref unregisterTarget so that the
 * bus's registry never sees a dangling target pointer.
 *
 * Ownership and lifetime:
 *   - The control block outlives the bus exactly long enough for any
 *     remaining weak tokens to be destroyed cleanly. It is always
 *     allocated on the heap and only reached through a shared/weak
 *     pointer pair.
 *   - @ref allocateSlot records a raw, non-owning pointer to the target.
 *     The caller (the bus) must guarantee that the target outlives the
 *     corresponding token or that the token's destructor runs before the
 *     target is freed -- which is the invariant provided by
 *     @ref AbstractMessageTarget owning its tokens.
 *
 * Thread-safety: every entry point is safe to call from any thread.
 * Implementations typically protect the registry with a
 * @c std::shared_mutex and the alive flag with a @c std::atomic.
 */
class IBusControlBlock
{
  public:
    virtual ~IBusControlBlock() = default;

    /**
     * @brief Returns @c true when the bus is still alive.
     *
     * Reads must be cheap and wait-free. Implementations typically back
     * this with a @c std::atomic<bool> loaded with acquire ordering.
     */
    [[nodiscard]] virtual bool isAlive() const noexcept = 0;

    /**
     * @brief Marks the bus as dead.
     *
     * Called from the bus destructor. Idempotent: a second call is a
     * no-op. After @ref markDead returns, every future call to
     * @ref isAlive reports @c false and every future call to
     * @ref unregisterTarget is a no-op, regardless of the connection id.
     */
    virtual void markDead() noexcept = 0;

    /**
     * @brief Allocates a new registry slot for @p target and returns its
     *        generational id.
     *
     * The returned @ref ConnectionId always has a non-zero @c generation
     * when allocation succeeded. When the bus is already dead or when
     * the registry is exhausted, implementations return a default-
     * constructed sentinel (@c generation == 0) so that call sites can
     * detect the failure without needing a separate @ref Result.
     *
     * Passing @c nullptr is a programming error; implementations return
     * the sentinel id and do not insert anything into the registry.
     */
    [[nodiscard]] virtual ConnectionId
        allocateSlot(AbstractMessageTarget *target) = 0;

    /**
     * @brief Removes the registry entry addressed by @p id.
     *
     * Idempotent: unregistering a stale id or an id from a dead bus is a
     * no-op. After this call returns, no future dispatch on this bus
     * reaches the slot addressed by @p id; in-flight dispatches on that
     * slot are serialised separately by the token (see
     * @ref ConnectionToken).
     */
    virtual void unregisterTarget(ConnectionId id) noexcept = 0;

    IBusControlBlock(const IBusControlBlock &)            = delete;
    IBusControlBlock &operator=(const IBusControlBlock &) = delete;
    IBusControlBlock(IBusControlBlock &&)                 = delete;
    IBusControlBlock &operator=(IBusControlBlock &&)      = delete;

  protected:
    IBusControlBlock() = default;
};

} // namespace vigine::messaging

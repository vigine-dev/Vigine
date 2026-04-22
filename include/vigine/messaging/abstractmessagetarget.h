#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "vigine/messaging/targetkind.h"

namespace vigine::messaging
{
class IConnectionToken;
class IMessage;

/**
 * @brief Stateful abstract base for every object that receives messages
 *        from an @ref IMessageBus.
 *
 * @ref AbstractMessageTarget carries the one piece of state every
 * subscriber needs: a mutex-guarded vector of the tokens that keep its
 * bus subscriptions alive. Concrete subscribers (states, task flows,
 * tasks, topic nodes, channel nodes, actor nodes, application targets)
 * derive from this base, override the two pure-virtual hooks
 * (@ref targetKind and @ref onMessage), and rely on the base class's
 * RAII to unregister every subscription when the target is destroyed.
 *
 * The class carries state, so it follows the project's @c Abstract
 * naming convention rather than the @c I pure-virtual prefix. The
 * @c _connections vector is the RAII anchor that makes this base more
 * than a pure interface: every concrete target inherits the same
 * lifetime plumbing without having to duplicate it.
 *
 * Ownership: the vector owns the @ref IConnectionToken handles. Tokens
 * are produced by the bus during registration and handed over via
 * @ref acceptConnection; their destructors run when the vector clears
 * (either on target destruction or on a selective erase driven by the
 * target's owner). The bus keeps only a raw, non-owning pointer to the
 * target in its registry, which is why the target's address must be
 * stable while any connection is live.
 *
 * Move semantics: moving a registered target would leave a dangling
 * pointer inside every bus that tracks it. The move constructor and
 * move-assignment operator therefore assert that the source holds no
 * connections. Callers that need to relocate a target should either do
 * so before registration or destroy all tokens first (see
 * @ref canMove).
 *
 * Thread-safety: @ref acceptConnection takes an exclusive lock on an
 * internal mutex so that multiple buses can register the same target
 * concurrently. The lock is NOT held during @ref onMessage dispatch;
 * the bus is responsible for sequencing dispatch against the target's
 * own concurrency model.
 *
 * Reentrancy: implementations MUST NOT destroy the target or any of its
 * tokens from inside @ref onMessage. Doing so makes the
 * @ref ConnectionToken destructor wait for itself, which deadlocks.
 */
class AbstractMessageTarget
{
  public:
    virtual ~AbstractMessageTarget() = default;

    /**
     * @brief Returns the closed-enum tag describing this target's kind.
     *
     * Used by the bus to dispatch on delivery policy without a
     * @c dynamic_cast. The value is stable for the lifetime of the
     * target.
     */
    [[nodiscard]] virtual TargetKind targetKind() const noexcept = 0;

    /**
     * @brief Delivers @p message to the target.
     *
     * Called from the bus's dispatch path. Implementations must run to
     * completion without destroying the target or any of its
     * @ref IConnectionToken handles; see the class-level reentrancy
     * note.
     */
    virtual void onMessage(const IMessage &message) = 0;

    /**
     * @brief Takes ownership of @p token and appends it to the
     *        target's connection list.
     *
     * Called by the bus at the end of a successful registration. The
     * call is serialised against other @ref acceptConnection calls on
     * the same target by @c _connectionsMutex, so a target can be
     * registered on multiple buses concurrently without risk of a
     * @c std::vector resize-race.
     */
    void acceptConnection(std::unique_ptr<IConnectionToken> token);

    /**
     * @brief Returns @c true when the target holds no active
     *        connections and can safely be moved.
     *
     * The snapshot reads the vector size without the mutex; callers
     * that race with a concurrent @ref acceptConnection should not rely
     * on the value for correctness.
     */
    [[nodiscard]] bool canMove() const noexcept;

    AbstractMessageTarget(const AbstractMessageTarget &)            = delete;
    AbstractMessageTarget &operator=(const AbstractMessageTarget &) = delete;

    /**
     * @brief Moves connections from @p other into @c *this.
     *
     * @p other MUST be unregistered (@ref canMove @c true). The
     * constructor asserts on a registered source in debug builds; in
     * release builds the assert is compiled out and the move proceeds
     * unsafely -- callers are responsible for the precondition.
     */
    AbstractMessageTarget(AbstractMessageTarget &&other) noexcept;

    /**
     * @brief Move-assigns from @p other.
     *
     * Both operands MUST be unregistered; see @ref canMove.
     */
    AbstractMessageTarget &operator=(AbstractMessageTarget &&other) noexcept;

  protected:
    AbstractMessageTarget() = default;

  private:
    std::vector<std::unique_ptr<IConnectionToken>> _connections;
    mutable std::mutex                             _connectionsMutex;
};

} // namespace vigine::messaging

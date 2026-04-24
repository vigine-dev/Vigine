#pragma once

/**
 * @file abstractengine_token.h
 * @brief Thin stateful base for @ref vigine::engine::IEngineToken that
 *        owns the bound-state handle and the alive flag.
 *
 * @ref vigine::engine::AbstractEngineToken is the @c Abstract tier of
 * the token recipe: it carries the minimal state every concrete
 * @c EngineToken implementation shares (@c boundState, @c alive) and
 * implements the two introspection accessors on top of that state.
 * Every other accessor -- gated and ungated alike -- stays pure
 * virtual; the concrete @c EngineToken (shipping in a follow-up
 * issue) delegates them to the root @ref vigine::IContext and closes
 * the inheritance chain.
 *
 * Encapsulation: data members are strictly private (INV-12). Derived
 * classes that need to transition the alive flag go through the
 * protected @ref markExpired setter; they never mutate the atomic
 * directly.
 */

#include <atomic>

#include "vigine/api/engine/iengine_token.h"
#include "vigine/statemachine/stateid.h"

namespace vigine::engine
{

/**
 * @brief Stateful abstract base for every concrete @c EngineToken.
 *
 * The class stores the two pieces of state every token needs:
 *   - @ref _boundState: the @ref vigine::statemachine::StateId the
 *     token was issued against. Immutable for the token's lifetime.
 *   - @ref _alive: an atomic flag flipped by the state machine's
 *     invalidation hook when the bound state transitions away.
 *
 * Naming uses the @c Abstract prefix per INV-10 because the class
 * implements @ref IEngineToken::boundState and
 * @ref IEngineToken::isAlive -- it carries both state and
 * non-pure-virtual bodies.
 *
 * Lifetime and thread-safety:
 *   - Tokens are constructed by the state machine when a task
 *     enters a state. The constructor stamps @ref _boundState and
 *     initialises @ref _alive to @c true.
 *   - @ref markExpired is called once, from the state machine's
 *     invalidation hook, when the bound state transitions away.
 *     A second call is a no-op.
 *   - @ref isAlive is safe to read from any thread at any time. The
 *     atomic uses release / acquire semantics so an observed
 *     @c false return happens-before any side effect the invalidation
 *     hook publishes.
 *
 * Scope: concrete subclasses must implement every accessor except
 * @ref boundState and @ref isAlive. The concrete @c EngineToken
 * lands in a follow-up issue together with the @ref vigine::IContext
 * factory.
 */
class AbstractEngineToken : public IEngineToken
{
  public:
    ~AbstractEngineToken() override = default;

    // ------ IEngineToken (implemented) ------

    [[nodiscard]] vigine::statemachine::StateId
        boundState() const noexcept override;

    [[nodiscard]] bool isAlive() const noexcept override;

    AbstractEngineToken(const AbstractEngineToken &)            = delete;
    AbstractEngineToken &operator=(const AbstractEngineToken &) = delete;
    AbstractEngineToken(AbstractEngineToken &&)                 = delete;
    AbstractEngineToken &operator=(AbstractEngineToken &&)      = delete;

  protected:
    /**
     * @brief Stamps @p boundState and arms the alive flag.
     *
     * Called by concrete subclasses during their construction. The
     * subclass is expected to hold a reference to the engine's
     * @ref vigine::IContext so the gated accessors can delegate
     * through it; that reference is not part of this base because
     * the exact wiring (direct reference, shared pointer to a
     * control block, etc.) belongs to the concrete implementation.
     */
    explicit AbstractEngineToken(vigine::statemachine::StateId boundState) noexcept;

    /**
     * @brief Flips @ref _alive to @c false.
     *
     * Idempotent: a second call is a no-op. The transition uses
     * release semantics so every prior write made by the state
     * machine's invalidation hook is visible to subsequent
     * @ref isAlive observers.
     */
    void markExpired() noexcept;

  private:
    vigine::statemachine::StateId _boundState;
    std::atomic<bool>             _alive{true};
};

// ---------------------------------------------------------------------------
// Inline definitions -- kept in-header because the class is a thin
// stateful base with no out-of-line implementation file in this leaf.
// A follow-up issue may move the bodies to a .cpp when the concrete
// EngineToken lands, at which point the CMakeLists SOURCES_ENGINE_TOKEN
// block takes shape.
// ---------------------------------------------------------------------------

inline AbstractEngineToken::AbstractEngineToken(
    vigine::statemachine::StateId boundState) noexcept
    : _boundState(boundState)
{
}

inline vigine::statemachine::StateId
    AbstractEngineToken::boundState() const noexcept
{
    return _boundState;
}

inline bool AbstractEngineToken::isAlive() const noexcept
{
    return _alive.load(std::memory_order_acquire);
}

inline void AbstractEngineToken::markExpired() noexcept
{
    _alive.store(false, std::memory_order_release);
}

} // namespace vigine::engine

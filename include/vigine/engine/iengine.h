#pragma once

#include "vigine/result.h"

namespace vigine
{
class IContext;
} // namespace vigine

namespace vigine::engine
{
/**
 * @brief Pure-virtual canonical entry point for the engine runtime.
 *
 * @ref IEngine wraps an @ref IContext aggregator and adds the lifecycle
 * primitives the engine needs beyond the aggregator's own
 * freeze-after-run contract:
 *
 *   - @ref run blocks the calling thread on the thread manager's
 *     main-thread pump until @ref shutdown is observed. @ref run calls
 *     @ref IContext::freeze on entry so topology mutation is rejected
 *     after the main loop starts.
 *   - @ref shutdown requests a clean stop of the main loop. It is safe
 *     to call from any thread (TSAN-clean) and it is idempotent: a
 *     second call is a no-op.
 *   - @ref isRunning reports whether @ref run has been entered and has
 *     not yet returned. Reads are lock-free and safe from any thread.
 *   - @ref context returns the engine-owned aggregator. The reference
 *     stays live for the engine's lifetime.
 *
 * Strict construction order (AD-5 C8, encoded on @ref AbstractEngine):
 *   1. Context aggregator (which internally builds the thread manager,
 *      system bus, and Level-1 wrappers in the documented order).
 *   2. Engine lifecycle state (shutdown flag, pump mutex, CV).
 *
 * Strict destruction order (reverse):
 *   1. Call @ref IContext::freeze if not already called, so late
 *      post-backs observe a closed topology.
 *   2. Drain the context (cascades to wrappers, then system bus, then
 *      thread manager as reverse members of the aggregator).
 *
 * Thread-safety: read-only accessors are safe from any thread at any
 * time. @ref run must be called exactly once from one thread (typically
 * the OS main thread). @ref shutdown may be called from any thread at
 * any time before or after @ref run.
 *
 * INV-1 compliance: no template parameters on the interface or any of
 * its methods. INV-10 compliance: the @c I prefix marks a pure-virtual
 * interface with no state and no non-virtual method bodies. INV-11
 * compliance: the public API exposes only @ref IContext and @ref Result;
 * no graph primitive type (@c NodeId, @c EdgeId, @c IGraph, ...) leaks
 * across the boundary.
 */
class IEngine
{
  public:
    virtual ~IEngine() = default;

    /**
     * @brief Returns the engine-owned context aggregator.
     *
     * The reference is valid for the entire engine lifetime. Callers
     * register services and create user buses through the returned
     * context between @ref createEngine and @ref run; after @ref run
     * calls @ref IContext::freeze on entry, subsequent mutators on the
     * returned context fail fast with @ref Result::Code::TopologyFrozen.
     */
    [[nodiscard]] virtual IContext &context() = 0;

    /**
     * @brief Blocks the calling thread on the main-thread pump until
     *        @ref shutdown is observed.
     *
     * Calls @ref IContext::freeze on entry. The call returns
     * @ref Result::Code::Success when the loop exits cleanly (i.e.
     * @ref shutdown was called) and @ref Result::Code::Error when
     * invoked more than once on the same engine instance (the
     * lifecycle is single-shot).
     *
     * @ref run must be called from a thread the caller is willing to
     * dedicate to pump work for the engine's lifetime; the canonical
     * caller is the OS main thread in a foreground engine.
     */
    [[nodiscard]] virtual Result run() = 0;

    /**
     * @brief Requests a clean stop of the main loop.
     *
     * Safe to call from any thread at any time. Idempotent: a second
     * call is a no-op. After @ref shutdown returns, any in-progress
     * @ref run call observes the request and returns
     * @ref Result::Code::Success after draining any pending main-thread
     * work.
     *
     * Calling @ref shutdown before @ref run has entered the loop is
     * allowed; it pre-arms the shutdown flag so the next @ref run call
     * returns immediately after freezing the context.
     */
    virtual void shutdown() noexcept = 0;

    /**
     * @brief Reports whether @ref run has been entered and has not yet
     *        returned.
     *
     * Safe to call from any thread at any time. The read is lock-free
     * and uses an acquire fence so a @c true result happens-before any
     * side effect observable from the main loop.
     */
    [[nodiscard]] virtual bool isRunning() const noexcept = 0;

    IEngine(const IEngine &)            = delete;
    IEngine &operator=(const IEngine &) = delete;
    IEngine(IEngine &&)                 = delete;
    IEngine &operator=(IEngine &&)      = delete;

  protected:
    IEngine() = default;
};

} // namespace vigine::engine

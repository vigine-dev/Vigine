#pragma once

#include <cstddef>
#include <memory>
#include <string_view>

#include "vigine/result.h"
#include "vigine/threading/irunnable.h"
#include "vigine/threading/itaskhandle.h"
#include "vigine/threading/namedthreadid.h"
#include "vigine/threading/threadaffinity.h"

namespace vigine::threading
{
/**
 * @brief Pure-virtual core of the threading substrate.
 *
 * @ref IThreadManager is the level-0 primitive that hides every concrete
 * thread and scheduling decision behind one typed surface. Callers hand
 * it an @ref IRunnable plus a @ref ThreadAffinity and get back an
 * @ref ITaskHandle. The manager decides which physical OS thread will
 * execute the runnable.
 *
 * The interface is deliberately orthogonal to the graph substrate: no
 * method mentions nodes, edges, or any wrapper-layer concept. Higher
 * layers (messaging, ECS, state machine, task flow) consume
 * @ref IThreadManager through an injected reference; they never
 * reach into its implementation details.
 *
 * Ownership and lifetime:
 *   - @ref schedule and @ref scheduleOnNamed take unique ownership of
 *     the @ref IRunnable and return a @ref ITaskHandle owned by the
 *     caller. The handle remains valid after the runnable completes.
 *   - @ref registerNamedThread returns a generational
 *     @ref NamedThreadId that stays stable across subsequent
 *     @ref scheduleOnNamed calls until @ref unregisterNamedThread is
 *     called for that id.
 *   - @ref shutdown drains every queue, joins every worker, and rejects
 *     subsequent @ref schedule / @ref scheduleOnNamed calls with an
 *     error @ref Result carried on the returned handle.
 *
 * Thread-safety: every entry point is safe to call from any thread at
 * any time before @ref shutdown completes. After shutdown, calls return
 * failure rather than block.
 */
class IThreadManager
{
  public:
    virtual ~IThreadManager() = default;

    // ------ Task scheduling ------

    /**
     * @brief Takes ownership of @p runnable and schedules it on the
     *        affinity-selected thread.
     *
     * A null @p runnable is a programming error; implementations return
     * a handle whose @ref ITaskHandle::wait reports an error @ref Result.
     */
    [[nodiscard]] virtual std::unique_ptr<ITaskHandle>
        schedule(std::unique_ptr<IRunnable> runnable,
                 ThreadAffinity             affinity = ThreadAffinity::Any) = 0;

    /**
     * @brief Takes ownership of @p runnable and schedules it on the
     *        named thread identified by @p named.
     *
     * A stale or invalid @p named returns a handle whose
     * @ref ITaskHandle::wait reports an error @ref Result; the runnable
     * is not executed.
     */
    [[nodiscard]] virtual std::unique_ptr<ITaskHandle>
        scheduleOnNamed(std::unique_ptr<IRunnable> runnable, NamedThreadId named) = 0;

    // ------ Main-thread integration ------

    /**
     * @brief Queues @p runnable for execution on the main thread.
     *
     * The runnable waits in an internal MPSC queue until the main thread
     * calls @ref runMainThreadPump. Ownership transfers to the manager.
     */
    virtual void postToMainThread(std::unique_ptr<IRunnable> runnable) = 0;

    /**
     * @brief Drains the main-thread queue, invoking every queued
     *        runnable on the calling thread.
     *
     * The engine's main loop is expected to call this once per tick.
     * Non-main-thread callers receive no error; the call simply drains
     * on the caller's thread because "main-thread" is a scheduling
     * contract, not a runtime identity check.
     */
    virtual void runMainThreadPump() = 0;

    // ------ Named thread registry ------

    /**
     * @brief Registers a new named thread and returns its generational
     *        identifier.
     *
     * The @p name is retained by the manager for diagnostics only; it
     * does not need to be unique, though duplicates may confuse
     * human-readable telemetry. Returns an invalid @ref NamedThreadId
     * (generation 0) when the @ref ThreadManagerConfig::maxNamedThreads
     * cap is reached or when the manager is already shut down.
     */
    [[nodiscard]] virtual NamedThreadId registerNamedThread(std::string_view name) = 0;

    /**
     * @brief Removes the named thread addressed by @p id.
     *
     * Any queued runnables on that thread are drained before the thread
     * exits. Subsequent @ref scheduleOnNamed calls with the stale id
     * return a failing handle. Removing an invalid id is a no-op.
     */
    virtual void unregisterNamedThread(NamedThreadId id) = 0;

    // ------ Observability ------

    /**
     * @brief Returns the number of worker threads in the pool.
     */
    [[nodiscard]] virtual std::size_t poolSize() const noexcept = 0;

    /**
     * @brief Returns the number of live dedicated threads currently
     *        allocated.
     */
    [[nodiscard]] virtual std::size_t dedicatedThreadCount() const noexcept = 0;

    /**
     * @brief Returns the number of live named threads currently
     *        registered.
     */
    [[nodiscard]] virtual std::size_t namedThreadCount() const noexcept = 0;

    // ------ Lifecycle ------

    /**
     * @brief Drains every queue, joins every worker, and moves the
     *        manager into the shut-down state.
     *
     * Idempotent: a second call is a no-op. After shutdown, all
     * @ref schedule / @ref scheduleOnNamed / @ref postToMainThread calls
     * return without executing the runnable; any outstanding handles
     * resolve with a failing @ref Result.
     */
    virtual void shutdown() = 0;

    IThreadManager(const IThreadManager &)            = delete;
    IThreadManager &operator=(const IThreadManager &) = delete;
    IThreadManager(IThreadManager &&)                 = delete;
    IThreadManager &operator=(IThreadManager &&)      = delete;

  protected:
    IThreadManager() = default;
};

} // namespace vigine::threading

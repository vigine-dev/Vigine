#pragma once

#include <memory>

#include "vigine/core/threading/abstractthreadmanager.h"
#include "vigine/core/threading/irunnable.h"
#include "vigine/core/threading/itaskhandle.h"
#include "vigine/core/threading/namedthreadid.h"
#include "vigine/core/threading/threadaffinity.h"
#include "vigine/core/threading/threadmanagerconfig.h"

namespace vigine::core::threading
{
/**
 * @brief Default concrete @ref IThreadManager implementation.
 *
 * Closes the inheritance chain on @ref AbstractThreadManager and adds
 * the scheduling mechanics: a worker pool of @c std::thread, a FIFO
 * queue per dedicated slot, a FIFO queue per named thread, and an
 * MPSC queue for main-thread post-backs drained by
 * @ref runMainThreadPump.
 *
 * Concurrency primitives used internally are strictly @c std::thread /
 * @c std::mutex / @c std::condition_variable / @c std::atomic so that
 * the implementation compiles unchanged on Windows, Linux, and macOS.
 * Platform-specific tuning (priority class, OS-level thread naming) is
 * deliberately deferred to a later leaf.
 *
 * The destructor runs a deterministic shutdown: it flips the shut-down
 * flag, notifies every worker, drains every queue (discarding any
 * runnables that had not started), and joins every thread before the
 * object returns. Explicit @ref shutdown is equivalent; calling it more
 * than once is a no-op.
 */
class ThreadManager final : public AbstractThreadManager
{
  public:
    explicit ThreadManager(ThreadManagerConfig config);
    ~ThreadManager() override;

    [[nodiscard]] std::unique_ptr<ITaskHandle>
        schedule(std::unique_ptr<IRunnable> runnable, ThreadAffinity affinity) override;

    [[nodiscard]] std::unique_ptr<ITaskHandle>
        scheduleOnNamed(std::unique_ptr<IRunnable> runnable, NamedThreadId named) override;

    void postToMainThread(std::unique_ptr<IRunnable> runnable) override;
    void runMainThreadPump() override;

    void shutdown() override;

    void unregisterNamedThread(NamedThreadId id) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace vigine::core::threading

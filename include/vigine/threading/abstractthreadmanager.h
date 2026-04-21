#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "vigine/result.h"
#include "vigine/threading/ibarrier.h"
#include "vigine/threading/imessagechannel.h"
#include "vigine/threading/imutex.h"
#include "vigine/threading/isemaphore.h"
#include "vigine/threading/ithreadmanager.h"
#include "vigine/threading/namedthreadid.h"
#include "vigine/threading/threadmanagerconfig.h"

namespace vigine::threading
{
/**
 * @brief Concrete stateful base for every in-process @ref IThreadManager.
 *
 * @ref AbstractThreadManager carries the shared state every concrete
 * thread manager needs: the resolved @ref ThreadManagerConfig, the
 * dedicated-thread slot table, the named-thread registry, and the
 * atomic shut-down flag. It implements the parts of @ref IThreadManager
 * that are identical across every implementation — registry bookkeeping
 * and observability — and leaves the scheduling and pump mechanics
 * abstract for @ref DefaultThreadManager to provide.
 *
 * The class carries state, so it follows the project's @c Abstract
 * naming convention rather than the @c I pure-virtual prefix. It is
 * abstract only in the logical sense — users do not instantiate it
 * directly; @ref createThreadManager returns a @c final subclass
 * (@ref DefaultThreadManager) that closes the inheritance chain.
 *
 * Thread-safety: every mutating entry point on the registry takes
 * an exclusive lock on an internal @c std::mutex. The shut-down flag
 * is an @c std::atomic inspected by every scheduling path so that
 * post-shutdown calls fail fast without needing the registry lock.
 * The observability counters (@ref dedicatedThreadCount,
 * @ref namedThreadCount) report a consistent snapshot by taking the
 * same lock as the mutators they shadow.
 */
class AbstractThreadManager : public IThreadManager
{
  public:
    ~AbstractThreadManager() override;

    // ------ IThreadManager: named registry (shared implementation) ------

    [[nodiscard]] NamedThreadId registerNamedThread(std::string_view name) override;
    void                        unregisterNamedThread(NamedThreadId id) override;

    // ------ IThreadManager: observability (shared implementation) ------

    [[nodiscard]] std::size_t poolSize() const noexcept override;
    [[nodiscard]] std::size_t dedicatedThreadCount() const noexcept override;
    [[nodiscard]] std::size_t namedThreadCount() const noexcept override;

    // ------ IThreadManager: sync primitive factories (shared impl) ------

    /**
     * @brief Default @ref IMutex factory returning a @c std::mutex
     *        wrapper.
     *
     * Derived classes override only when they need a different concrete
     * mutex (for example a sanitiser-instrumented one).
     */
    [[nodiscard]] std::unique_ptr<IMutex> createMutex() override;

    /**
     * @brief Default @ref ISemaphore factory returning a counter built
     *        on @c std::mutex + @c std::condition_variable.
     */
    [[nodiscard]] std::unique_ptr<ISemaphore>
        createSemaphore(std::size_t initialCount) override;

    /**
     * @brief Default @ref IBarrier factory returning a cv-based
     *        reusable barrier.
     *
     * A dedicated cv-based implementation (not @c std::barrier) is used
     * for ABI stability across compilers and to keep the fallback code
     * path singular — every supported toolchain hits exactly one
     * implementation.
     */
    [[nodiscard]] std::unique_ptr<IBarrier>
        createBarrier(std::size_t parties) override;

    /**
     * @brief Default @ref IMessageChannel factory returning a bounded
     *        FIFO queue.
     *
     * The queue uses @c std::mutex plus two @c std::condition_variable
     * instances (not-full / not-empty) to keep producer and consumer
     * wake-ups independent.
     */
    [[nodiscard]] std::unique_ptr<IMessageChannel>
        createMessageChannel(std::size_t capacity) override;

  protected:
    explicit AbstractThreadManager(ThreadManagerConfig config) noexcept;

    /**
     * @brief Read-only view of the resolved configuration.
     *
     * The returned reference is valid for the lifetime of the manager.
     * Hardware-concurrency substitution (@c 0 → @c hardware_concurrency)
     * has already been applied by the constructor.
     */
    [[nodiscard]] const ThreadManagerConfig &config() const noexcept;

    /**
     * @brief Atomically reports whether @ref shutdown has been entered.
     *
     * Scheduling paths on derived classes consult this flag first and
     * fail fast when it is set, without acquiring any registry lock.
     */
    [[nodiscard]] bool isShutDown() const noexcept;

    /**
     * @brief Marks the manager as shut-down.
     *
     * Called by derived @ref shutdown implementations exactly once as
     * the first step of their shutdown sequence, so that concurrent
     * schedule callers immediately take the failure path.
     */
    void markShutDown() noexcept;

    /**
     * @brief Bumps the live dedicated-thread counter.
     *
     * Derived classes call this when they lazily allocate a dedicated
     * thread for a caller; the matching @ref releaseDedicatedSlot call
     * drops the counter.
     */
    void acquireDedicatedSlot() noexcept;

    /**
     * @brief Drops the live dedicated-thread counter by one.
     *
     * Safe to call more times than @ref acquireDedicatedSlot — the
     * counter saturates at zero.
     */
    void releaseDedicatedSlot() noexcept;

    /**
     * @brief Resolves a name to a registered named-thread slot index.
     *
     * Derived schedulers translate a @ref NamedThreadId carried by a
     * @ref scheduleOnNamed call into a slot index they can consult
     * directly. Returns @c SIZE_MAX when the id is stale.
     */
    [[nodiscard]] std::size_t resolveNamedSlot(NamedThreadId id) const noexcept;

    /**
     * @brief Returns the diagnostic name associated with @p id.
     *
     * Empty string when the id is stale.
     */
    [[nodiscard]] std::string namedThreadName(NamedThreadId id) const;

  private:
    struct NamedSlot
    {
        std::string   name;
        std::uint32_t generation{0};
        bool          live{false};
    };

    ThreadManagerConfig            _config;
    mutable std::mutex             _registryMutex;
    std::vector<NamedSlot>         _namedSlots;
    std::size_t                    _namedLiveCount{0};
    std::atomic<std::size_t>       _dedicatedCount{0};
    std::atomic<bool>              _shutDown{false};
    std::atomic<std::uint32_t>     _nextNamedGeneration{1};
};

} // namespace vigine::threading

#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>

#include "vigine/api/engine/engineconfig.h"
#include "vigine/api/engine/iengine.h"

namespace vigine
{
class IContext;
} // namespace vigine

namespace vigine::engine
{
/**
 * @brief Stateful abstract base that owns every engine-wide resource
 *        exposed through @ref IEngine.
 *
 * @ref AbstractEngine is the stateful layer of the @ref IEngine recipe:
 * it owns the context aggregator and encodes the strict construction
 * and destruction order required by AD-5 C8. A concrete closer (see
 * @ref Engine in @c include/vigine/impl/engine) seals the inheritance
 * chain so the factory can hand out @c std::unique_ptr<IEngine>.
 *
 * The class carries state, so it uses the project's @c Abstract naming
 * convention. All data members are @c private per the strict
 * encapsulation rule; derived classes that need to inspect the
 * substrate go through @c protected accessors.
 *
 * Construction order (encoded by member declaration order below):
 *   1. @ref _context -- the context aggregator. Created first because
 *      every downstream lifecycle primitive depends on observing a
 *      live aggregator. The aggregator itself internally builds the
 *      thread manager first, then the system bus, then the Level-1
 *      wrappers (see @ref context::AbstractContext for details).
 *   2. @ref _runMode -- hint captured from the config for future
 *      diagnostics; lifecycle-neutral.
 *   3. Lifecycle flags and sync primitives -- @ref _shutdownRequested,
 *      @ref _running, @ref _runEntered, @ref _pumpMutex, @ref _pumpCv.
 *      These live past the context so @ref shutdown may be called
 *      safely during the context's destruction.
 *
 * Destruction is the reverse of the above because C++ tears down
 * members in reverse declaration order. The lifecycle state dies first,
 * then the context dies (which cascades to its wrappers, then the
 * system bus, then the thread manager last).
 *
 * Lifecycle contract:
 *   - @ref run is single-shot. Calling it a second time returns
 *     @ref Result::Code::Error; the caller is expected to drop the
 *     engine and build a new one.
 *   - @ref shutdown sets an atomic flag (under @ref _pumpMutex so the
 *     flag flip is atomic with respect to a pump wait) and notifies
 *     @ref _pumpCv so a sleeping main loop wakes immediately. It is
 *     idempotent: subsequent calls set the same flag to the same value
 *     and re-notify (a cheap no-op when no-one is waiting).
 *   - @ref isRunning observes @ref _running with an acquire fence so a
 *     @c true result happens-before any side effect observable from
 *     the main loop.
 *
 * INV-11 compliance: the header imports the @ref engineconfig and
 * @ref iengine public headers plus standard-library synchronisation
 * primitives. It does not include any graph header and does not
 * mention @c NodeId, @c EdgeId, @c IGraph, or any graph visitor type.
 */
class AbstractEngine : public IEngine
{
  public:
    ~AbstractEngine() override;

    // ------ IEngine ------

    [[nodiscard]] IContext     &context() override;
    [[nodiscard]] vigine::Result run() override;
    void                        shutdown() noexcept override;
    [[nodiscard]] bool      isRunning() const noexcept override;

    AbstractEngine(const AbstractEngine &)            = delete;
    AbstractEngine &operator=(const AbstractEngine &) = delete;
    AbstractEngine(AbstractEngine &&)                 = delete;
    AbstractEngine &operator=(AbstractEngine &&)      = delete;

  protected:
    /**
     * @brief Constructs the engine with the strict ctor order described
     *        on the class docstring.
     *
     * Step 1 builds the context aggregator from @p config.context via
     * @ref context::createContext. Step 2 captures @p config.runMode.
     * Step 3 default-initialises the lifecycle flags and sync
     * primitives (their default values clear the flags and build an
     * unlocked mutex + empty-wait CV).
     *
     * If any construction step throws, RAII unwinds the steps that
     * already completed in reverse order: partial state never escapes
     * the constructor.
     */
    explicit AbstractEngine(const EngineConfig &config);

    /**
     * @brief Returns the pump tick duration used by @ref run to bound
     *        wait-wakeup latency when no post-back arrives.
     *
     * The tick is deliberately short (a few milliseconds) so a
     * @ref shutdown call from any thread is observed quickly without
     * burning CPU. Derived classes may override the value for test
     * harnesses that need tighter bounds; the default suits production
     * and the smoke suite.
     */
    [[nodiscard]] virtual unsigned pumpTickMilliseconds() const noexcept;

    /**
     * @brief Returns the run-mode hint captured at construction time.
     *
     * Exposed as @c protected so derived closers can branch on the
     * value for diagnostics (e.g. dial back logging in @c Test mode)
     * without round-tripping through @ref EngineConfig. The public
     * API does not surface the run mode because it is advisory: the
     * lifecycle behaviour is identical across modes at this leaf.
     */
    [[nodiscard]] RunMode runMode() const noexcept;

  private:
    // ------ Strict ctor order encoded by member declaration order ------

    /**
     * @brief First member: the context aggregator.
     *
     * Declared first because every lifecycle primitive below expects
     * to observe a live aggregator. Destructed last because C++ tears
     * down members in reverse declaration order; by the time @c _context
     * dies, all lifecycle flags have been cleared and any pump thread
     * has already returned from @ref run.
     */
    std::unique_ptr<IContext> _context;

    /**
     * @brief Second member: run-mode hint captured from the config.
     *
     * Stored for diagnostics only; the lifecycle flow does not branch
     * on the value at this leaf. Kept as a plain enum so the class
     * stays trivially destructible at this layer.
     */
    RunMode _runMode;

    // ------ Lifecycle state (guarded by _pumpMutex) ------

    /**
     * @brief Shutdown request flag.
     *
     * Set by @ref shutdown; read by the main loop under
     * @ref _pumpMutex. Atomic so callers that want a quick unlocked
     * peek (e.g. diagnostics) can do so without blocking the mutex.
     * The loop's authoritative read takes the mutex so it cannot miss
     * a simultaneous @ref shutdown / CV-notify pair.
     */
    std::atomic<bool> _shutdownRequested{false};

    /**
     * @brief Running flag.
     *
     * Set to @c true as @ref run enters the loop and cleared as
     * @ref run exits. Atomic so @ref isRunning reads lock-free. The
     * transition is strictly owned by the single thread that called
     * @ref run so there is no need for a CAS.
     */
    std::atomic<bool> _running{false};

    /**
     * @brief Single-shot guard: set by @ref run on its first entry.
     *
     * Subsequent calls to @ref run observe the flag and return an
     * error @ref Result without mutating any state. The flag lives
     * past the matching @ref run return so a second call is rejected
     * even after the first exits normally.
     */
    std::atomic<bool> _runEntered{false};

    /**
     * @brief Mutex guarding @ref _shutdownRequested against the main
     *        loop's wait-for-notify.
     *
     * Held only briefly by @ref shutdown (to flip the flag under the
     * CV's monitor) and by the main loop (while consulting the
     * predicate inside the CV wait). Never held across a pump tick or
     * any caller-supplied runnable, so shutdown latency is bounded by
     * pump-tick duration, not by arbitrary user code.
     */
    mutable std::mutex _pumpMutex;

    /**
     * @brief Condition variable waking the main loop when
     *        @ref shutdown is called.
     *
     * Notified once per @ref shutdown invocation under
     * @ref _pumpMutex so the wake is monotonic with the flag flip. The
     * loop also wakes periodically on its own timer so any main-thread
     * post-back that bypasses the CV is still drained within one pump
     * tick.
     */
    std::condition_variable _pumpCv;
};

} // namespace vigine::engine

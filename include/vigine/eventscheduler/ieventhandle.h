#pragma once

namespace vigine::eventscheduler
{

/**
 * @brief Pure-virtual RAII handle for a scheduled event.
 *
 * @ref IEventHandle is the cancellation token returned by
 * @ref IEventScheduler::schedule. Destroying the handle is equivalent to
 * calling @ref cancel — the event is deactivated and no further fires
 * will be delivered (in-flight delivery may still complete; see the
 * soft-cancel semantics in Q-FE5).
 *
 * Callers hold the handle as @c std::unique_ptr<IEventHandle>; the
 * scheduler retains no reference to the handle after returning it. When
 * the handle goes out of scope the associated event stops firing.
 *
 * INV-1: no template parameters in the public surface.
 * INV-10: @c I prefix for a pure-virtual interface with no state.
 * INV-11: no graph types in this header.
 */
class IEventHandle
{
  public:
    virtual ~IEventHandle() = default;

    /**
     * @brief Soft-cancels the event.
     *
     * Marks the event inactive. Any in-flight delivery completes;
     * future timer fires or OS signal deliveries are prevented. Calling
     * @ref cancel on an already-inactive handle is a no-op.
     */
    virtual void cancel() noexcept = 0;

    /**
     * @brief Returns @c true when the event is still armed and active.
     *
     * Returns @c false after @ref cancel or after the configured
     * @c count has been reached.
     */
    [[nodiscard]] virtual bool active() const noexcept = 0;

    IEventHandle(const IEventHandle &)            = delete;
    IEventHandle &operator=(const IEventHandle &) = delete;
    IEventHandle(IEventHandle &&)                 = delete;
    IEventHandle &operator=(IEventHandle &&)      = delete;

  protected:
    IEventHandle() = default;
};

} // namespace vigine::eventscheduler

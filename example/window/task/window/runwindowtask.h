#pragma once

#include <vigine/api/taskflow/abstracttask.h>
#include <vigine/api/ecs/platform/iwindoweventhandler.h>

#include <atomic>
#include <memory>

namespace vigine::messaging
{
class ISubscriptionToken;
} // namespace vigine::messaging

/**
 * @brief Window-driving task: thin event router that forwards every
 *        platform callback to @c TextEditorService.
 *
 * The task owns no DI handles, no cached services, no interaction
 * state — every dependency is resolved through @ref apiToken at the
 * call site, and every piece of state that used to live as a member
 * here has moved into @c TextEditorComponent (managed by
 * @c TextEditorSystem behind @c TextEditorService).
 *
 * Token-expiration cooperation: @ref run subscribes a one-shot
 * callback through @ref apiToken()->subscribeExpiration. When the FSM
 * transitions away from @c InitState the callback flips
 * @ref _shutdownRequested; the per-frame callback observes the flag,
 * stops issuing new render commands, and asks the platform service to
 * close the window so the blocking @c showWindow call returns.
 */
class RunWindowTask final : public vigine::AbstractTask
{
  public:
    RunWindowTask();

    [[nodiscard]] vigine::Result run() override;

    void onMouseButtonDown(vigine::ecs::platform::MouseButton button, int x, int y);
    void onMouseButtonUp(vigine::ecs::platform::MouseButton button, int x, int y);
    void onMouseMove(int x, int y);
    void onMouseWheel(int delta, int x, int y);
    void onKeyDown(const vigine::ecs::platform::KeyEvent &event);
    void onKeyUp(const vigine::ecs::platform::KeyEvent &event);
    void onChar(const vigine::ecs::platform::TextEvent &event);

  private:
    // Single member: the FSM-driven token-expiration subscription.
    // Held here (not on the system) because the subscription's
    // lifetime tracks the task's run() invocation, not the editor's.
    // Atomic flag below cooperates with the per-frame callback so a
    // late FSM transition observes the close request without racing
    // the render path.
    std::atomic<bool> _shutdownRequested{false};
    std::unique_ptr<vigine::messaging::ISubscriptionToken> _expirationSubscription;
};

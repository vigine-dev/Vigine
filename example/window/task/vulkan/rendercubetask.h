#pragma once

#include <vigine/api/messaging/isubscriptiontoken.h>
#include <vigine/api/taskflow/abstracttask.h>

#include <atomic>
#include <memory>

class RenderCubeTask final : public vigine::AbstractTask
{
  public:
    RenderCubeTask();

    [[nodiscard]] vigine::Result run() override;

  private:
    float _rotationAngle{0.0f};
    /**
     * @brief One-shot expiration subscription installed by @ref run.
     *
     * The cube task is the WorkState's representative of the FSM-driven
     * render path; while it is trivial in this example, the GPU cleanup
     * pattern that a real render task would need lives here. The
     * expiration callback flips @ref _expired to @c true so the next
     * @ref run tick observes the flag and short-circuits without
     * touching the renderer.
     */
    std::unique_ptr<vigine::messaging::ISubscriptionToken> _expirationSubscription;
    std::atomic<bool> _expired{false};
};

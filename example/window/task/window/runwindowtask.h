#pragma once

#include <vigine/api/taskflow/abstracttask.h>
#include <vigine/api/ecs/platform/iwindoweventhandler.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <glm/vec3.hpp>
#include <memory>

namespace vigine
{
class Entity;

namespace ecs
{
namespace platform
{
class WindowComponent;
} // namespace platform
namespace graphics
{
class RenderSystem;
} // namespace graphics
} // namespace ecs

namespace messaging
{
class ISubscriptionToken;
} // namespace messaging
} // namespace vigine

/**
 * @brief Window-driving task for the modern FSM-pumped engine.
 *
 * Every dependency is resolved through @ref apiToken at the call site:
 * the entity manager, platform / graphics services, signal emitter,
 * engine back-ref, and app-scope text-editor service all flow through
 * the engine token without any task-side caching.
 *
 * Task-private state covers only what the task needs to track across
 * event callbacks fired during the platform showWindow loop — focused
 * entity, drag state, mouse-ray helper handles, resize bookkeeping,
 * the modifier-key bitmask, plus the one-shot expiration subscription
 * token that ties the task's lifetime to the FSM-driven invalidation.
 *
 * Token-expiration cooperation: on entry @ref run subscribes a one-shot
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
    enum MovementKeyMask : uint8_t
    {
        MoveKeyW = 1u << 0,
        MoveKeyA = 1u << 1,
        MoveKeyS = 1u << 2,
        MoveKeyD = 1u << 3,
        MoveKeyQ = 1u << 4,
        MoveKeyE = 1u << 5,
    };

    void onWindowResized(vigine::ecs::platform::WindowComponent *window, int width, int height);
    void updateCameraMovementKey(unsigned int keyCode, bool pressed);
    bool handleClipboardShortcut(const vigine::ecs::platform::KeyEvent &event);
    bool isFocusedTextEditor() const;
    void setFocusedEntity(vigine::Entity *entity);
    bool ensureMouseRayEntity();
    bool ensureMouseClickSphereEntity();
    void updateMouseRayVisualization(int x, int y);
    void updateMouseClickSphereVisualization(int x, int y);
    bool beginObjectDrag(vigine::Entity *entity, int x, int y);
    void updateObjectDrag(int x, int y, bool suppressZDelta = true);
    void endObjectDrag();

    // Task-private state. None of these are DI-resolved; they track the
    // task's own bookkeeping across event callbacks fired during the
    // platform showWindow loop.
    vigine::Entity *_focusedEntity{nullptr};
    vigine::Entity *_mouseRayEntity{nullptr};
    vigine::Entity *_mouseClickSphereEntity{nullptr};
    vigine::Entity *_dragEntity{nullptr};
    glm::vec3 _focusedOriginalScale{1.0f, 1.0f, 1.0f};
    bool _hasFocusedOriginalScale{false};
    bool _mouseRayVisible{true};
    bool _hasMouseRaySample{false};
    int _lastMouseRayX{0};
    int _lastMouseRayY{0};
    bool _ctrlHeld{false};
    bool _objectDragActive{false};
    bool _dragEditorGroup{false};
    float _dragDistanceFromCamera{0.0f};
    glm::vec3 _dragGrabOffset{0.0f, 0.0f, 0.0f};
    vigine::ecs::platform::WindowComponent *_pendingResizeWindow{nullptr};
    uint32_t _pendingResizeWidth{0};
    uint32_t _pendingResizeHeight{0};
    uint32_t _appliedResizeWidth{0};
    uint32_t _appliedResizeHeight{0};
    bool _resizePending{false};
    uint8_t _movementKeyMask{0};
    std::chrono::steady_clock::time_point _lastResizeEvent{};
    std::chrono::steady_clock::time_point _lastResizeApply{};
    // Token-expiration cooperation flag. The expiration callback flips
    // it to @c true on FSM transition out of the bound state; the
    // per-frame callback reads it on every tick and asks the platform
    // service to close the window so @c showWindow returns and the
    // task exits cleanly. Atomic because the callback runs on the FSM
    // controller thread while the frame callback runs on the engine
    // pump thread.
    std::atomic<bool> _shutdownRequested{false};
    // Held until @ref run returns so the FSM stops invoking the
    // expiration callback once the task itself has finished. Reset
    // explicitly at the end of the run path even though the destructor
    // would also drop it; keeping the lifetime explicit makes the
    // ordering relative to the cached service pointers obvious.
    std::unique_ptr<vigine::messaging::ISubscriptionToken> _expirationSubscription;
};

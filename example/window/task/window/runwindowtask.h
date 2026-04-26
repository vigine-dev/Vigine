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
class EntityManager;
class Entity;

namespace ecs
{
namespace platform
{
class PlatformService;
class WindowComponent;
} // namespace platform
namespace graphics
{
class GraphicsService;
class RenderSystem;
} // namespace graphics
} // namespace ecs

namespace engine
{
class IEngine;
} // namespace engine

namespace messaging
{
class ISignalEmitter;
class ISubscriptionToken;
} // namespace messaging
} // namespace vigine

class TextEditorSystem;

/**
 * @brief Window-driving task for the modern FSM-pumped engine.
 *
 * Every dependency the task needs (entity manager, platform / graphics
 * services, render system, engine back-ref, signal emitter, app-scope
 * text-editor service) is resolved through @ref apiToken at the call
 * site. The task carries no DI-result members — only task-private state
 * (focused entity, drag state, resize bookkeeping, the one-shot
 * expiration subscription token).
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

    /**
     * @brief Per-call resolution snapshot of every engine-known
     *        dependency the task touches.
     *
     * Each member is a non-owning handle resolved fresh through
     * @ref apiToken at the call site. A method that needs any subset
     * checks the relevant pointer for null before dereferencing — the
     * resolver tolerates a missing token / failed dynamic_cast and
     * returns null pointers for the unresolved slots so callers can
     * skip the work without surfacing an error to the FSM.
     *
     * Lifetime: the snapshot is short-lived. The pointers themselves
     * resolve to engine-default services that live for the engine's
     * entire lifetime, so the snapshot stays valid for as long as the
     * caller keeps it on the stack — but each method re-resolves to
     * keep the "no caching" rule visible at every touchpoint.
     */
    struct Deps
    {
        vigine::EntityManager                  *entityManager{nullptr};
        vigine::ecs::platform::PlatformService *platformService{nullptr};
        vigine::ecs::graphics::GraphicsService *graphicsService{nullptr};
        vigine::ecs::graphics::RenderSystem    *renderSystem{nullptr};
        vigine::engine::IEngine                *engine{nullptr};
        vigine::messaging::ISignalEmitter      *signalEmitter{nullptr};
        std::shared_ptr<TextEditorSystem>       textEditorSystem;
    };

    /**
     * @brief Resolves every engine-known dependency through the
     *        current @ref apiToken in a single call.
     *
     * Returns a snapshot with non-null members for every successfully
     * resolved slot and null for the rest. A null token, an expired
     * gated accessor, or an unexpected-type dynamic_cast each leave
     * their respective slot null without touching the others.
     */
    [[nodiscard]] Deps resolveDeps() const;

    void onWindowResized(vigine::ecs::platform::WindowComponent *window, int width, int height);
    void updateCameraMovementKey(vigine::ecs::graphics::RenderSystem *renderSystem,
                                 unsigned int                          keyCode,
                                 bool                                  pressed);
    bool handleClipboardShortcut(const std::shared_ptr<TextEditorSystem> &textEditorSystem,
                                 const vigine::ecs::platform::KeyEvent   &event);
    bool isFocusedTextEditor(const std::shared_ptr<TextEditorSystem> &textEditorSystem) const;
    void setFocusedEntity(const Deps &deps, vigine::Entity *entity);
    bool ensureMouseRayEntity(const Deps &deps);
    bool ensureMouseClickSphereEntity(const Deps &deps);
    void updateMouseRayVisualization(const Deps &deps, int x, int y);
    void updateMouseClickSphereVisualization(const Deps &deps, int x, int y);
    bool beginObjectDrag(const Deps &deps, vigine::Entity *entity, int x, int y);
    void updateObjectDrag(const Deps &deps, int x, int y, bool suppressZDelta = true);
    void endObjectDrag();

    // Task-private state. None of these are DI-resolved; they track the
    // task's own bookkeeping across event callbacks fired during the
    // platform showWindow loop.
    vigine::Entity *_mainWindowEntity{nullptr};
    vigine::Entity *_focusedEntity{nullptr};
    vigine::Entity *_mouseRayEntity{nullptr};
    vigine::Entity *_mouseClickSphereEntity{nullptr};
    glm::vec3 _focusedOriginalScale{1.0f, 1.0f, 1.0f};
    bool _hasFocusedOriginalScale{false};
    bool _mouseRayVisible{true};
    bool _hasMouseRaySample{false};
    int _lastMouseRayX{0};
    int _lastMouseRayY{0};
    bool _ctrlHeld{false};
    bool _objectDragActive{false};
    bool _dragEditorGroup{false};
    vigine::Entity *_dragEntity{nullptr};
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

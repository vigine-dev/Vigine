#pragma once

#include <vigine/api/taskflow/abstracttask.h>
#include <vigine/api/ecs/platform/iwindoweventhandler.h>
#include <vigine/api/messaging/isignalemitter.h>
#include <vigine/api/service/serviceid.h>

#include "../../system/texteditorsystem.h"

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
} // namespace vigine

/**
 * @brief Window-driving task for the modern FSM-pumped engine.
 *
 * The task is constructed before @c IEngine::run with non-owning handles
 * to the legacy @c EntityManager and to the platform / graphics service
 * @ref vigine::service::ServiceId values stamped at registration time.
 * Each tick the task resolves the services through @ref apiToken()->service
 * (the gated state-scoped accessor on @ref vigine::engine::IEngineToken),
 * shows the platform window, and pumps the per-frame render callback
 * until the window closes.
 *
 * Token-expiration cooperation: on entry @ref run subscribes a one-shot
 * callback through @ref apiToken()->subscribeExpiration. When the FSM
 * transitions away from @c InitState the callback flips
 * @ref _shutdownRequested; the per-frame callback observes the flag,
 * stops issuing new render commands, and asks the platform service to
 * close the window so the blocking @c showWindow call returns. The flag
 * also guards every dereference of the cached service pointers so a late
 * frame callback that runs after the token expires turns into a no-op
 * instead of touching freed state.
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

    void setEntityManager(vigine::EntityManager *entityManager) noexcept;
    void setPlatformServiceId(vigine::service::ServiceId id) noexcept;
    void setGraphicsServiceId(vigine::service::ServiceId id) noexcept;
    void setEngine(vigine::engine::IEngine *engine) noexcept;
    void setTextEditorSystem(std::shared_ptr<TextEditorSystem> editorSystem);
    void setSignalEmitter(vigine::messaging::ISignalEmitter *emitter) noexcept;

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

    bool resolveServices();
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

    vigine::EntityManager *_entityManager{nullptr};
    vigine::service::ServiceId _platformServiceId{};
    vigine::service::ServiceId _graphicsServiceId{};
    vigine::engine::IEngine *_engine{nullptr};
    vigine::messaging::ISignalEmitter *_signalEmitter{nullptr};
    vigine::ecs::platform::PlatformService *_platformService{nullptr};
    vigine::ecs::graphics::GraphicsService *_graphicsService{nullptr};
    vigine::ecs::graphics::RenderSystem *_renderSystem{nullptr};
    std::shared_ptr<TextEditorSystem> _textEditorSystem;
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

#pragma once

#include <vigine/api/taskflow/abstracttask.h>
#include <vigine/api/ecs/platform/iwindoweventhandler.h>
#include <vigine/api/messaging/isignalemitter.h>

#include "../../system/texteditorsystem.h"

#include <chrono>
#include <cstdint>
#include <glm/vec3.hpp>
#include <memory>

namespace vigine
{
namespace ecs
{
namespace platform
{
class PlatformService;
class WindowComponent;
} // namespace platform
} // namespace ecs
class Entity;
namespace ecs
{
namespace graphics
{
class GraphicsService;
class RenderSystem;
} // namespace graphics
} // namespace ecs
} // namespace vigine

class RunWindowTask final : public vigine::AbstractTask
{
  public:
    RunWindowTask();

    void contextChanged() override;
    vigine::Result execute() override;

    void onMouseButtonDown(vigine::ecs::platform::MouseButton button, int x, int y);
    void onMouseButtonUp(vigine::ecs::platform::MouseButton button, int x, int y);
    void onMouseMove(int x, int y);
    void onMouseWheel(int delta, int x, int y);
    void onKeyDown(const vigine::ecs::platform::KeyEvent &event);
    void onKeyUp(const vigine::ecs::platform::KeyEvent &event);
    void onChar(const vigine::ecs::platform::TextEvent &event);

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

    vigine::messaging::ISignalEmitter *_signalEmitter{nullptr};
    vigine::ecs::platform::PlatformService *_platformService{nullptr};
    vigine::ecs::graphics::GraphicsService *_graphicsService{nullptr};
    vigine::ecs::graphics::RenderSystem *_renderSystem{nullptr};
    std::shared_ptr<TextEditorSystem> _textEditorSystem;
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
};

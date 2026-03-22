#pragma once

#include "windoweventsignal.h"

#include <vigine/abstracttask.h>
#include <vigine/ecs/platform/iwindoweventhandler.h>

#include "../../system/texteditorsystem.h"

#include <chrono>
#include <cstdint>
#include <glm/vec3.hpp>
#include <memory>

namespace vigine
{
namespace platform
{
class PlatformService;
class WindowComponent;
} // namespace platform
class Entity;
namespace graphics
{
class GraphicsService;
class RenderSystem;
} // namespace graphics
} // namespace vigine

class RunWindowTask : public vigine::AbstractTask,
                      public IMouseEventSignalEmiter,
                      public IKeyEventSignalEmiter
{
  public:
    RunWindowTask();

    void contextChanged() override;
    vigine::Result execute() override;

    void onMouseButtonDown(vigine::platform::MouseButton button, int x, int y);
    void onMouseButtonUp(vigine::platform::MouseButton button, int x, int y);
    void onMouseMove(int x, int y);
    void onMouseWheel(int delta, int x, int y);
    void onKeyDown(const vigine::platform::KeyEvent &event);
    void onKeyUp(const vigine::platform::KeyEvent &event);
    void onChar(const vigine::platform::TextEvent &event);

    void setTextEditorSystem(std::shared_ptr<TextEditorSystem> editorSystem);

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

    void onWindowResized(vigine::platform::WindowComponent *window, int width, int height);
    void updateCameraMovementKey(unsigned int keyCode, bool pressed);
    bool handleClipboardShortcut(const vigine::platform::KeyEvent &event);
    bool isFocusedTextEditor() const;
    void setFocusedEntity(vigine::Entity *entity);
    bool ensureMouseRayEntity();
    bool ensureMouseClickSphereEntity();
    void updateMouseRayVisualization(int x, int y);
    void updateMouseClickSphereVisualization(int x, int y);
    bool beginObjectDrag(vigine::Entity *entity, int x, int y);
    void updateObjectDrag(int x, int y, bool suppressZDelta = true);
    void endObjectDrag();

    vigine::platform::PlatformService *_platformService{nullptr};
    vigine::graphics::GraphicsService *_graphicsService{nullptr};
    vigine::graphics::RenderSystem *_renderSystem{nullptr};
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
    vigine::platform::WindowComponent *_pendingResizeWindow{nullptr};
    uint32_t _pendingResizeWidth{0};
    uint32_t _pendingResizeHeight{0};
    uint32_t _appliedResizeWidth{0};
    uint32_t _appliedResizeHeight{0};
    bool _resizePending{false};
    uint8_t _movementKeyMask{0};
    std::chrono::steady_clock::time_point _lastResizeEvent{};
    std::chrono::steady_clock::time_point _lastResizeApply{};
};

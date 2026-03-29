#pragma once

#include "windoweventsignal.h"

#include <vigine/abstracttask.h>
#include <vigine/ecs/platform/inputmap.h>
#include <vigine/ecs/platform/inputprofilecomponent.h>
#include <vigine/ecs/platform/iwindoweventhandler.h>
#include <vigine/ecs/platform/profiles/blenderclassicprofile.h>
#include <vigine/ecs/platform/profiles/blendermodernprofile.h>
#include <vigine/ecs/platform/profiles/cinema4dprofile.h>
#include <vigine/ecs/platform/profiles/godotprofile.h>
#include <vigine/ecs/platform/profiles/max3dsprofile.h>
#include <vigine/ecs/platform/profiles/mayaprofile.h>
#include <vigine/ecs/platform/profiles/sourceengineprofile.h>
#include <vigine/ecs/platform/profiles/unityprofile.h>
#include <vigine/ecs/platform/profiles/unrealprofile.h>

#include "../../service/uiservice.h"
#include "../../system/manipulationsystem.h"
#include "../../system/selectionsystem.h"
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

    // Input configuration
    void setNumpadEmulation(bool enabled);
    void setEmulate3ButtonMouse(bool enabled);

    // Input profile API
    void setActiveProfile(vigine::platform::InputProfileComponent *profile);
    vigine::platform::InputProfileComponent *activeProfile() const;

  private:
    void onWindowResized(vigine::platform::WindowComponent *window, int width, int height);
    void updateContinuousActions(unsigned int keyCode, unsigned int modifiers, bool pressed);
    void resetAllContinuousActions();
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
    SelectionSystem _selectionSystem;
    std::unique_ptr<ManipulationSystem> _manipulationSystem;
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
    bool _altHeld{false};
    bool _shiftHeld{false};
    bool _objectDragActive{false};
    // Emulate 3-Button Mouse state (Phase 4)
    bool _emulatedOrbitActive{false};
    bool _emulatedZoomDragActive{false};
    int _emulatedDragStartY{0};
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
    vigine::platform::BlenderClassicProfile _defaultProfile;
    vigine::platform::BlenderModernProfile _blenderModernProfile;
    vigine::platform::MayaProfile _mayaProfile;
    vigine::platform::Max3dsProfile _max3dsProfile;
    vigine::platform::UnrealProfile _unrealProfile;
    vigine::platform::UnityProfile _unityProfile;
    vigine::platform::SourceEngineProfile _sourceEngineProfile;
    vigine::platform::GodotProfile _godotProfile;
    vigine::platform::Cinema4DProfile _cinema4dProfile;
    vigine::platform::InputProfileComponent *_activeProfile{&_defaultProfile};
    bool _numpadEmulation{false};
    bool _emulate3ButtonMouse{false};
    std::unique_ptr<UIService> _uiService;
    vigine::Entity *_windowEntity{nullptr};
    std::chrono::steady_clock::time_point _lastResizeEvent{};
    std::chrono::steady_clock::time_point _lastResizeApply{};
};

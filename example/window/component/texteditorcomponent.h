#pragma once

#include <chrono>
#include <cstdint>
#include <glm/vec3.hpp>

namespace vigine
{
class Entity;
} // namespace vigine

/**
 * @file texteditorcomponent.h
 * @brief Single ECS component bundling every piece of editor-window
 *        interaction state.
 *
 * The editor window does more than route keystrokes into the text
 * buffer: it tracks which entity is focused, which entity is being
 * dragged, where the last mouse click landed, whether Ctrl is held,
 * whether a swapchain resize is pending, and so on. All of that
 * state used to live on the @c RunWindowTask object as raw members;
 * this component pulls it into a single ECS-shaped struct so the
 * task itself can stay state-free and the @c TextEditorSystem can
 * own / drive the state through its event-handler methods.
 *
 * Lifetime: the component is created by the editor-setup task and
 * registered against an entity through @c TextEditorSystem::bindInteractionEntity.
 * The system keeps a single live binding (the example wires it to
 * the @c MainWindow entity) and queries it on every event call.
 *
 * Encapsulation: pure value aggregate. Public fields are the entire
 * surface; the component exists to ferry data between the system's
 * event handlers, not to enforce invariants. ENCAP EXEMPT in the
 * style of @c vigine::service::ServiceId.
 */
// ENCAP EXEMPT: pure value aggregate
struct TextEditorComponent
{
    // ---- Focus state ---------------------------------------------------
    vigine::Entity *focusedEntity{nullptr};
    glm::vec3       focusedOriginalScale{1.0f, 1.0f, 1.0f};
    bool            hasFocusedOriginalScale{false};

    // ---- Mouse-ray helper (visualises last clicked screen point) ------
    vigine::Entity *mouseRayEntity{nullptr};
    bool            mouseRayVisible{true};
    bool            hasMouseRaySample{false};
    int             lastMouseRayX{0};
    int             lastMouseRayY{0};

    // ---- Mouse-click-sphere helper ------------------------------------
    vigine::Entity *mouseClickSphereEntity{nullptr};

    // ---- Modifier / movement keys -------------------------------------
    bool    ctrlHeld{false};
    uint8_t movementKeyMask{0};

    // ---- Object drag (one entity at a time) ---------------------------
    bool             objectDragActive{false};
    bool             objectDragEditorGroup{false};
    vigine::Entity  *objectDragEntity{nullptr};
    float            objectDragDistanceFromCamera{0.0f};
    glm::vec3        objectDragGrabOffset{0.0f, 0.0f, 0.0f};

    // ---- Resize tracking (debounced apply) -----------------------------
    uint32_t                              pendingResizeWidth{0};
    uint32_t                              pendingResizeHeight{0};
    uint32_t                              appliedResizeWidth{0};
    uint32_t                              appliedResizeHeight{0};
    bool                                  resizePending{false};
    std::chrono::steady_clock::time_point lastResizeEvent{};
    std::chrono::steady_clock::time_point lastResizeApply{};
};

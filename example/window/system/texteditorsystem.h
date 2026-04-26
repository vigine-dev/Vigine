#pragma once

#include <vigine/api/ecs/platform/iwindoweventhandler.h>
#include <vigine/impl/ecs/graphics/textcomponent.h>

#include "../component/texteditorcomponent.h"
#include "../texteditstate.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace vigine
{
class EntityManager;
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

/**
 * @brief Editor / interaction system that owns the @c TextEditState
 *        text buffer and the per-entity @c TextEditorComponent UI
 *        state, and that drives every window-event hook
 *        @c RunWindowTask used to handle directly.
 *
 * The system used to expose a small set of editor-only event hooks
 * (@ref onChar, @ref onKeyDown, @ref onMouseWheel ...). It now also
 * absorbs the focus / object-drag / mouse-ray / mouse-click-sphere /
 * Ctrl-tracking / movement-key / debounced-resize state @c RunWindowTask
 * used to carry as raw members. The state lives in
 * @ref TextEditorComponent records bound through
 * @ref bindInteractionEntity; @ref RunWindowTask drops to a thin
 * event router that resolves @ref TextEditorService and forwards
 * each callback verbatim.
 *
 * Wiring: @ref bind connects the system to the engine's entity
 * manager + graphics service / render system. Setup tasks in the
 * example (@c SetupTextEditTask) call @ref bindInteractionEntity
 * for the @c MainWindow entity after the editor visuals are built;
 * the system stores per-entity @ref TextEditorComponent records
 * keyed by raw entity pointer.
 *
 * Threading: all event handlers run on whatever thread the platform
 * pump fires the underlying @c WindowEventHandler callback on (the
 * Win32 message-pump thread on Windows). The system carries no
 * synchronisation; callers serialise their event dispatch.
 */
class TextEditorSystem
{
  public:
    explicit TextEditorSystem(std::shared_ptr<TextEditState> state);

    /**
     * @brief Wires the editor system to the entity manager and the
     *        graphics service / render system pair.
     */
    void bind(vigine::EntityManager *entityManager,
              vigine::ecs::graphics::GraphicsService *graphicsService,
              vigine::ecs::graphics::RenderSystem *renderSystem);

    void setLayout(std::size_t maxColumns, float panelWidth, float panelHeight);

    void onFrame();
    void onEditorClick(int x, int y);
    bool onMouseButtonDown(int x, int y, const vigine::Entity *pickedEntity);
    void onMouseButtonUp();
    void onMouseMove(int x, int y);
    void onKeyDown(unsigned int keyCode);
    void onChar(const vigine::ecs::platform::TextEvent &event, uint8_t movementKeyMask);
    void onMouseWheel(int delta); // delta > 0: scroll up; delta < 0: scroll down
    void setFocused(bool focused);
    void offsetEditorFrame(float dx, float dy, float dz = 0.0f);
    void refreshEditorLayout();

    bool isEditorEntity(const vigine::Entity *entity) const;

    void insertUtf8(const std::string &utf8);
    void clearText();

    [[nodiscard]] const std::string &text() const;

    // ====================================================================
    //  Window-interaction surface
    //
    //  The methods below absorb the per-event logic @c RunWindowTask used
    //  to carry directly (focus tracking, object-drag math, mouse-ray
    //  visualization, modifier-key bookkeeping, debounced swapchain
    //  resize). The system reads / writes a @ref TextEditorComponent
    //  record bound through @ref bindInteractionEntity; the @c RunWindowTask
    //  side becomes a thin router that forwards each callback to the
    //  matching method below.
    // ====================================================================

    /**
     * @brief Registers a fresh @ref TextEditorComponent against
     *        @p entity (or replaces any previously-bound component
     *        for that entity).
     *
     * The @c MainWindow entity is the canonical binding in the
     * example; the system keeps one live record per registered
     * entity. Returns the component reference so the setup task can
     * fill in the @c mouseRayEntity / @c mouseClickSphereEntity
     * pointers and any other entity handles the runtime path needs.
     */
    TextEditorComponent &bindInteractionEntity(vigine::Entity *entity);

    /**
     * @brief Returns the bound @ref TextEditorComponent for @p entity,
     *        or @c nullptr when no component was bound under that
     *        entity.
     */
    [[nodiscard]] TextEditorComponent *interaction(vigine::Entity *entity);
    [[nodiscard]] const TextEditorComponent *
        interaction(const vigine::Entity *entity) const;

    /**
     * @brief Returns the single live binding the example wires to
     *        @c MainWindow, or @c nullptr when no entity has been
     *        bound yet.
     *
     * The example registers exactly one interaction component (against
     * the @c MainWindow entity) and then drives every event through
     * the no-arg accessor; multi-window callers iterate
     * @ref interaction explicitly per entity.
     */
    [[nodiscard]] TextEditorComponent *interaction();
    [[nodiscard]] const TextEditorComponent *interaction() const;

    // ---- Window event router (operates on the bound interaction) ------

    void routeMouseButtonDown(vigine::ecs::platform::MouseButton button, int x, int y);
    void routeMouseButtonUp(vigine::ecs::platform::MouseButton button, int x, int y);
    void routeMouseMove(int x, int y);
    void routeMouseWheel(int delta, int x, int y);
    void routeKeyDown(const vigine::ecs::platform::KeyEvent &event);
    void routeKeyUp(const vigine::ecs::platform::KeyEvent &event);
    void routeChar(const vigine::ecs::platform::TextEvent &event);
    void routeWindowResized(int width, int height);

    /**
     * @brief Returns the focused entity from the bound component
     *        (@c nullptr when nothing is focused or no component is
     *        bound).
     */
    [[nodiscard]] vigine::Entity *focusedEntity() const;

    /**
     * @brief Returns the movement-key bitmask snapshot, or @c 0 when
     *        no component is bound.
     *
     * Exposed so callers that emit signals on key-down (e.g. @c onChar
     * forwarding from the platform pump) can pass the current
     * movement state into the editor's text-input pipeline without
     * re-reading the component.
     */
    [[nodiscard]] uint8_t movementKeyMask() const;

  private:
    void applyScrollOffset(float newOffset);
    void updateScrollbarVisuals();
    bool screenYToWorldY(int x, int y, float &worldY) const;
    bool isScrollbarEntity(const vigine::Entity *entity) const;
    void updateFocusFrameVisual(bool visible);

    std::size_t cursorFromHitPoint(int x, int y) const;

    static std::size_t nextUtf8Pos(const std::string &text, std::size_t pos);
    static bool isContinuationByte(unsigned char byte);

    // ---- Window-interaction helpers (work on the bound component) ----

    /**
     * @brief Sets the focused entity on the bound component, applying
     *        the scale-up visual on the new focus and restoring the
     *        previous focus's original scale.
     *
     * Editor entities (those that satisfy @ref isEditorEntity) receive
     * focus without the scale bump.
     */
    void setFocusedEntityImpl(TextEditorComponent &interaction, vigine::Entity *entity);

    /**
     * @brief Lazily creates / refreshes the mouse-ray visualization
     *        entity referenced by the bound component, then updates
     *        its transform to a ray cast from the camera through
     *        screen pixel @p (x, y).
     */
    void updateMouseRayVisualization(TextEditorComponent &interaction, int x, int y);
    void updateMouseClickSphereVisualization(TextEditorComponent &interaction, int x, int y);
    bool ensureMouseRayEntity(TextEditorComponent &interaction);
    bool ensureMouseClickSphereEntity(TextEditorComponent &interaction);

    /**
     * @brief Begins dragging @p entity from screen pixel @p (x, y).
     *
     * Computes the distance + grab offset, stores them on the
     * component, and flags whether the dragged entity is part of the
     * editor entity group.
     */
    bool beginObjectDrag(TextEditorComponent &interaction, vigine::Entity *entity, int x, int y);
    void updateObjectDrag(TextEditorComponent &interaction, int x, int y, bool suppressZDelta);
    void endObjectDrag(TextEditorComponent &interaction);

    /**
     * @brief Maps a Win32-style virtual key code to its movement /
     *        sprint binding and updates the camera-drive flags + the
     *        component's movement-key bitmask.
     */
    void updateCameraMovementKey(TextEditorComponent &interaction, unsigned int keyCode,
                                 bool pressed);

    /**
     * @brief Applies the pending swapchain resize stored on @p interaction
     *        when at least 80 ms have passed since the last resize
     *        event, then renders the next frame.
     *
     * Called from @ref onFrame so the render-system tick observes the
     * already-applied size when it draws.
     */
    void applyPendingResizeIfPaused(TextEditorComponent &interaction);

    bool handleClipboardShortcut(const vigine::ecs::platform::KeyEvent &event);

  private:
    std::shared_ptr<TextEditState> _state;
    vigine::EntityManager *_entityManager{nullptr};
    vigine::ecs::graphics::GraphicsService *_graphicsService{nullptr};
    vigine::ecs::graphics::RenderSystem *_renderSystem{nullptr};

    std::chrono::steady_clock::time_point _lastBlink{std::chrono::steady_clock::now()};
    std::vector<vigine::ecs::graphics::CursorSlot> _cursorSlots;
    float _panelWidth{4.8f};
    float _panelHeight{1.5f};
    float _panelCenterX{0.0f};
    float _panelTopY{2.4f};
    float _panelZBase{0.0f};    // cumulative Z offset (from wheel/depth drag)
    float _scrollOffsetY{0.0f}; // world units scrolled down (0 = top)
    float _contentHeight{0.0f}; // last known text content height (world units)
    float _scrollbarThumbCenterY{0.0f};
    float _scrollbarThumbHeight{0.0f};
    bool _scrollbarDragging{false};
    float _scrollbarDragYOffset{0.0f};
    bool _focused{false};

    // Per-entity interaction component records. The example wires
    // exactly one (against MainWindow); multi-window callers add
    // more through @ref bindInteractionEntity.
    std::unordered_map<vigine::Entity *, TextEditorComponent> _interactions;
};

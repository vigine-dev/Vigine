#pragma once

#include <vigine/api/ecs/platform/iwindoweventhandler.h>
#include <vigine/impl/ecs/graphics/textcomponent.h>

#include "../texteditstate.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
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

class TextEditorSystem
{
  public:
    explicit TextEditorSystem(std::shared_ptr<TextEditState> state);

    /**
     * @brief Wires the editor system to the entity manager and the
     *        graphics service / render system pair.
     *
     * The example holds the entity manager directly because the engine
     * front door does not surface it through @ref vigine::IContext.
     * The signature therefore takes the entity manager as its own
     * parameter.
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

  private:
    void applyScrollOffset(float newOffset);
    void updateScrollbarVisuals();
    bool screenYToWorldY(int x, int y, float &worldY) const;
    bool isScrollbarEntity(const vigine::Entity *entity) const;
    void updateFocusFrameVisual(bool visible);

    std::size_t cursorFromHitPoint(int x, int y) const;

    static std::size_t nextUtf8Pos(const std::string &text, std::size_t pos);
    static bool isContinuationByte(unsigned char byte);

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
};

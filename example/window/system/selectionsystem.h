#pragma once

#include <glm/vec3.hpp>
#include <vector>

namespace vigine
{
class Entity;
namespace graphics
{
class GraphicsService;
} // namespace graphics
} // namespace vigine

// Utility class for tracking object selection state.
// Not a core AbstractSystem — does not require bindEntity() or createComponents().
class SelectionSystem
{
  public:
    // Single-select: clears previous selection then selects entity.
    // Passing nullptr clears selection.
    void select(vigine::Entity *entity);

    // Toggle: adds if not selected, removes if already selected.
    void toggleSelection(vigine::Entity *entity);

    // Add entity to the selection set without clearing others.
    void addToSelection(vigine::Entity *entity);

    // Remove entity from the selection set.
    void removeFromSelection(vigine::Entity *entity);

    // Select all pickable entities from a provided list.
    void selectAll(const std::vector<vigine::Entity *> &allEntities);

    // Clear the entire selection.
    void deselectAll();

    // The primary selected entity (first in the set, used as orbit target).
    vigine::Entity *primarySelected() const;

    // All currently selected entities.
    const std::vector<vigine::Entity *> &selectedEntities() const;

    // Hover state (no-commit highlight).
    void setHovered(vigine::Entity *entity);
    vigine::Entity *hoveredEntity() const;

    // Compute world-space AABB center and half-diagonal radius for selected entities.
    // Returns false if no entities are selected or if positions are unavailable.
    // Returns false if no entities selected or transforms unavailable.
    bool computeSelectionBounds(glm::vec3 &outCenter, float &outRadius,
                                vigine::graphics::GraphicsService *graphicsService) const;

  private:
    std::vector<vigine::Entity *> _selected;
    vigine::Entity *_hovered{nullptr};
};

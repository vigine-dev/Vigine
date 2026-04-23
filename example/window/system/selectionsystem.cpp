#include "selectionsystem.h"

#include <vigine/ecs/entity.h>
#include <vigine/ecs/render/rendercomponent.h>
#include <vigine/ecs/render/transformcomponent.h>
#include <vigine/service/graphicsservice.h>

#include <algorithm>
#include <limits>

void SelectionSystem::select(vigine::Entity *entity)
{
    _selected.clear();
    if (entity)
        _selected.push_back(entity);
}

void SelectionSystem::toggleSelection(vigine::Entity *entity)
{
    if (!entity)
        return;

    const auto it = std::find(_selected.begin(), _selected.end(), entity);
    if (it != _selected.end())
        _selected.erase(it);
    else
        _selected.push_back(entity);
}

void SelectionSystem::addToSelection(vigine::Entity *entity)
{
    if (!entity)
        return;

    if (std::find(_selected.begin(), _selected.end(), entity) == _selected.end())
        _selected.push_back(entity);
}

void SelectionSystem::removeFromSelection(vigine::Entity *entity)
{
    const auto it = std::find(_selected.begin(), _selected.end(), entity);
    if (it != _selected.end())
        _selected.erase(it);
}

void SelectionSystem::selectAll(const std::vector<vigine::Entity *> &allEntities)
{
    _selected = allEntities;
}

void SelectionSystem::deselectAll() { _selected.clear(); }

vigine::Entity *SelectionSystem::primarySelected() const
{
    return _selected.empty() ? nullptr : _selected.front();
}

const std::vector<vigine::Entity *> &SelectionSystem::selectedEntities() const { return _selected; }

void SelectionSystem::setHovered(vigine::Entity *entity) { _hovered = entity; }
vigine::Entity *SelectionSystem::hoveredEntity() const { return _hovered; }

bool SelectionSystem::computeSelectionBounds(
    glm::vec3 &outCenter, float &outRadius,
    vigine::graphics::GraphicsService *graphicsService) const
{
    if (_selected.empty() || !graphicsService)
        return false;

    glm::vec3 minBounds(std::numeric_limits<float>::max());
    glm::vec3 maxBounds(std::numeric_limits<float>::lowest());

    bool hasAny = false;
    for (auto *e : _selected)
    {
        if (!e)
            continue;

        graphicsService->bindEntity(e);
        const auto *rc = graphicsService->renderComponent();
        if (rc)
        {
            const glm::vec3 pos   = rc->getTransform().getPosition();
            const glm::vec3 scale = rc->getTransform().getScale();
            const float halfExt   = std::max({scale.x, scale.y, scale.z, 0.5f});
            minBounds.x           = std::min(minBounds.x, pos.x - halfExt);
            minBounds.y           = std::min(minBounds.y, pos.y - halfExt);
            minBounds.z           = std::min(minBounds.z, pos.z - halfExt);
            maxBounds.x           = std::max(maxBounds.x, pos.x + halfExt);
            maxBounds.y           = std::max(maxBounds.y, pos.y + halfExt);
            maxBounds.z           = std::max(maxBounds.z, pos.z + halfExt);
            hasAny                = true;
        }
        graphicsService->unbindEntity();
    }

    if (!hasAny)
        return false;

    outCenter = (minBounds + maxBounds) * 0.5f;
    outRadius = glm::length(maxBounds - outCenter);
    return true;
}

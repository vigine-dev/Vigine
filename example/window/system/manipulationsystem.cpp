#include "manipulationsystem.h"

#include <vigine/ecs/entity.h>
#include <vigine/ecs/entitymanager.h>
#include <vigine/ecs/render/meshcomponent.h>
#include <vigine/ecs/render/rendercomponent.h>
#include <vigine/ecs/render/rendersystem.h>
#include <vigine/ecs/render/shadercomponent.h>
#include <vigine/ecs/render/transformcomponent.h>
#include <vigine/service/graphicsservice.h>

#include <glm/glm.hpp>

ManipulationSystem::ManipulationSystem(vigine::graphics::GraphicsService *graphicsService,
                                       vigine::graphics::RenderSystem *renderSystem,
                                       vigine::EntityManager *entityManager)
    : _graphicsService(graphicsService), _renderSystem(renderSystem), _entityManager(entityManager)
{
}

bool ManipulationSystem::isActive() const { return _state.mode != TransformMode::None; }
TransformMode ManipulationSystem::mode() const { return _state.mode; }
CoordSystem ManipulationSystem::coordSystem() const { return _state.coordSystem; }

// ─── Helpers ────────────────────────────────────────────────────────────────

void ManipulationSystem::applyTransform(const glm::vec3 &position, const glm::vec3 &rotation,
                                        const glm::vec3 &scale)
{
    if (!_targetEntity || !_graphicsService)
        return;

    _graphicsService->bindEntity(_targetEntity);
    if (auto *rc = _graphicsService->renderComponent())
    {
        auto tr = rc->getTransform();
        tr.setPosition(position);
        tr.setRotation(rotation);
        tr.setScale(scale);
        rc->setTransform(tr);
    }
    _graphicsService->unbindEntity();
}

glm::vec3 ManipulationSystem::applyAxisConstraint(const glm::vec3 &delta) const
{
    switch (_state.constraint)
    {
    case AxisConstraint::X:
        return glm::vec3(delta.x, 0.0f, 0.0f);
    case AxisConstraint::Y:
        return glm::vec3(0.0f, delta.y, 0.0f);
    case AxisConstraint::Z:
        return glm::vec3(0.0f, 0.0f, delta.z);
    default:
        return delta;
    }
}

// ─── Begin operations ────────────────────────────────────────────────────────

static ManipulationState snapshotEntity(vigine::Entity *entity,
                                        vigine::graphics::GraphicsService *gs)
{
    ManipulationState s;
    if (!entity || !gs)
        return s;

    gs->bindEntity(entity);
    if (const auto *rc = gs->renderComponent())
    {
        s.initialPosition = rc->getTransform().getPosition();
        s.initialRotation = rc->getTransform().getRotation();
        s.initialScale    = rc->getTransform().getScale();
    }
    gs->unbindEntity();
    return s;
}

void ManipulationSystem::beginGrab(vigine::Entity *entity, int mouseX, int mouseY)
{
    if (!entity)
        return;
    _targetEntity      = entity;
    _state             = snapshotEntity(entity, _graphicsService);
    _state.mode        = TransformMode::Grab;
    _state.constraint  = AxisConstraint::None;
    _state.startMouseX = mouseX;
    _state.startMouseY = mouseY;
    _state.lastMouseX  = mouseX;
    _state.lastMouseY  = mouseY;
}

void ManipulationSystem::setAxisConstraint(AxisConstraint constraint)
{
    _state.constraint = constraint;
}

// ─── Update ──────────────────────────────────────────────────────────────────

void ManipulationSystem::updateFromMouse(int mouseX, int mouseY)
{
    if (!isActive() || !_targetEntity)
        return;

    const int dx      = mouseX - _state.lastMouseX;
    const int dy      = mouseY - _state.lastMouseY;
    _state.lastMouseX = mouseX;
    _state.lastMouseY = mouseY;

    switch (_state.mode)
    {
    case TransformMode::Grab: {
        // Move on camera-parallel plane: map pixel delta → world units
        constexpr float kPixelToUnit = 0.005f;
        glm::vec3 worldDelta(static_cast<float>(dx) * kPixelToUnit,
                             -static_cast<float>(dy) * kPixelToUnit, 0.0f);
        worldDelta = applyAxisConstraint(worldDelta);

        _graphicsService->bindEntity(_targetEntity);
        if (auto *rc = _graphicsService->renderComponent())
        {
            auto tr     = rc->getTransform();
            glm::vec3 p = tr.getPosition() + worldDelta;
            tr.setPosition(p);
            rc->setTransform(tr);
        }
        _graphicsService->unbindEntity();
        break;
    }
    default:
        break;
    }
}

// ─── Confirm / Cancel ────────────────────────────────────────────────────────

void ManipulationSystem::confirm()
{
    _targetEntity = nullptr;
    _state        = ManipulationState{};
}

void ManipulationSystem::cancel()
{
    if (_targetEntity)
        applyTransform(_state.initialPosition, _state.initialRotation, _state.initialScale);

    _targetEntity = nullptr;
    _state        = ManipulationState{};
}

// ─── Duplicate / Delete ───────────────────────────────────────────────────────

std::vector<vigine::Entity *>
ManipulationSystem::duplicateEntities(const std::vector<vigine::Entity *> &entities, int mouseX,
                                      int mouseY)
{
    std::vector<vigine::Entity *> newEntities;

    if (entities.empty() || !_entityManager || !_graphicsService)
        return newEntities;

    for (auto *src : entities)
    {
        if (!src)
            continue;

        _graphicsService->bindEntity(src);
        const auto *srcRc = _graphicsService->renderComponent();
        if (!srcRc)
        {
            _graphicsService->unbindEntity();
            continue;
        }

        const auto srcTransform     = srcRc->getTransform();
        const auto srcMesh          = srcRc->getMesh();
        const auto srcShader        = srcRc->getShader();
        const auto srcTextureHandle = srcRc->textureHandle();
        const bool srcPickable      = srcRc->isPickable();
        _graphicsService->unbindEntity();

        auto *newEntity = _entityManager->createEntity();
        if (!newEntity)
            continue;

        _graphicsService->bindEntity(newEntity);
        // GraphicsService::bindEntity() auto-creates render components
        if (auto *rc = _graphicsService->renderComponent())
        {
            auto t = srcTransform;
            // Offset by 1 world unit so the duplicate is clearly separated from the original.
            t.setPosition(srcTransform.getPosition() + glm::vec3(1.0f, 0.0f, 0.0f));
            rc->setTransform(t);
            // For procedural meshes, create a fresh MeshComponent to avoid copying CPU vertex
            // data and GPU buffer handles from the source entity.
            if (srcMesh.isProceduralInShader())
            {
                vigine::graphics::MeshComponent freshMesh;
                freshMesh.setProceduralInShader(true, srcMesh.proceduralVertexCount());
                rc->setMesh(freshMesh);
            } else
            {
                rc->setMesh(srcMesh);
            }
            rc->setShader(srcShader);
            rc->setTextureHandle(srcTextureHandle);
            rc->setPickable(srcPickable);
        }
        _graphicsService->unbindEntity();

        newEntities.push_back(newEntity);
    }

    // Enter Grab for first duplicated entity
    if (!newEntities.empty())
        beginGrab(newEntities.front(), mouseX, mouseY);

    return newEntities;
}

void ManipulationSystem::deleteEntities(const std::vector<vigine::Entity *> &entities)
{
    if (entities.empty() || !_entityManager)
        return;

    confirm(); // deactivate any active manipulation

    for (auto *e : entities)
    {
        if (!e)
            continue;
        _entityManager->removeEntity(e);
    }
}

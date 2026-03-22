#include "vigine/ecs/render/rendersystem.h"

#include "vigine/ecs/entity.h"
#include "vigine/ecs/render/rendercomponent.h"
#include "vigine/ecs/render/vulkanapi.h"

#include <algorithm>
#include <iostream>
#include <limits>

namespace
{
void expandAabb(glm::vec3 &minBounds, glm::vec3 &maxBounds, const glm::vec3 &point)
{
    minBounds.x = (std::min)(minBounds.x, point.x);
    minBounds.y = (std::min)(minBounds.y, point.y);
    minBounds.z = (std::min)(minBounds.z, point.z);
    maxBounds.x = (std::max)(maxBounds.x, point.x);
    maxBounds.y = (std::max)(maxBounds.y, point.y);
    maxBounds.z = (std::max)(maxBounds.z, point.z);
}

bool intersectRayAabb(const glm::vec3 &origin, const glm::vec3 &direction, const glm::vec3 &minB,
                      const glm::vec3 &maxB, float &hitT)
{
    float tMin = 0.0f;
    float tMax = (std::numeric_limits<float>::max)();

    for (int axis = 0; axis < 3; ++axis)
    {
        const float dir = direction[axis];
        if (std::abs(dir) < 1e-6f)
        {
            if (origin[axis] < minB[axis] || origin[axis] > maxB[axis])
                return false;
            continue;
        }

        const float invDir = 1.0f / dir;
        float t0           = (minB[axis] - origin[axis]) * invDir;
        float t1           = (maxB[axis] - origin[axis]) * invDir;
        if (t0 > t1)
            std::swap(t0, t1);

        tMin = (std::max)(tMin, t0);
        tMax = (std::min)(tMax, t1);
        if (tMax < tMin)
            return false;
    }

    hitT = tMin;
    return true;
}

bool buildEntityAabb(const vigine::graphics::RenderComponent &rc, glm::vec3 &minBounds,
                     glm::vec3 &maxBounds)
{
    minBounds             = glm::vec3((std::numeric_limits<float>::max)());
    maxBounds             = glm::vec3((std::numeric_limits<float>::lowest)());

    const auto &transform = rc.getTransform();
    const glm::mat4 model = transform.getModelMatrix();
    const auto &mesh      = rc.getMesh();
    const auto &vertices  = mesh.getVertices();

    if (!vertices.empty())
    {
        for (const auto &v : vertices)
        {
            const glm::vec3 world = glm::vec3(model * glm::vec4(v.position, 1.0f));
            expandAabb(minBounds, maxBounds, world);
        }
        return true;
    }

    const auto &text = rc.getText();
    if (text.enabled() && !text.voxelOffsets().empty())
    {
        const float half = (std::max)(text.voxelSize() * 0.6f, 0.002f);
        for (const auto &offset : text.voxelOffsets())
        {
            const glm::vec3 center = glm::vec3(model * glm::vec4(offset, 1.0f));
            expandAabb(minBounds, maxBounds, center + glm::vec3(half));
            expandAabb(minBounds, maxBounds, center - glm::vec3(half));
        }
        return true;
    }

    const glm::vec3 center = transform.getPosition();
    const glm::vec3 half(0.5f);
    minBounds = center - half;
    maxBounds = center + half;
    return true;
}
} // namespace

using namespace vigine::graphics;

RenderSystem::RenderSystem(const SystemName &name)
    : AbstractSystem(name), _vulkanAPI(std::make_unique<VulkanAPI>()),
      _boundEntityComponent(nullptr)
{
    // Initialize Vulkan API
    if (!_vulkanAPI->initializeInstance())
    {
        std::cerr << "Failed to initialize Vulkan instance" << std::endl;
    }

    if (!_vulkanAPI->selectPhysicalDevice())
    {
        std::cerr << "Failed to select physical device" << std::endl;
    }

    if (!_vulkanAPI->createLogicalDevice())
    {
        std::cerr << "Failed to create logical device" << std::endl;
    }
}

RenderSystem::~RenderSystem()
{
    _entityComponents.clear();
    _vulkanAPI.reset();
}

bool RenderSystem::hasComponents(Entity *entity) const
{
    if (!entity)
        return false;

    return _entityComponents.find(entity) != _entityComponents.end();
}

void RenderSystem::createComponents(Entity *entity)
{
    if (!entity)
        return;

    if (hasComponents(entity))
        return;

    auto renderComponent      = std::make_unique<RenderComponent>();
    _entityComponents[entity] = std::move(renderComponent);
}

void RenderSystem::destroyComponents(Entity *entity)
{
    if (!entity)
        return;

    auto it = _entityComponents.find(entity);
    if (it != _entityComponents.end())
    {
        if (_boundEntityComponent == it->second.get())
            _boundEntityComponent = nullptr;

        _entityComponents.erase(it);
    }
}

RenderComponent *RenderSystem::boundRenderComponent() const { return _boundEntityComponent; }

vigine::SystemId RenderSystem::id() const { return "Render"; }

void RenderSystem::markGlyphDirty() { _glyphDirty = true; }

void RenderSystem::update()
{
    std::vector<glm::mat4> cubeModelMatrices;
    std::vector<glm::mat4> textVoxelModelMatrices;
    std::vector<glm::mat4> panelModelMatrices;
    std::vector<glm::mat4> glyphModelMatrices;
    std::vector<glm::mat4> sphereModelMatrices;
    const bool collectSdf = _glyphDirty;
    std::vector<GlyphQuadVertex> sdfGlyphVertices;
    const std::vector<uint8_t> *sdfAtlasPixels = nullptr;
    uint32_t sdfAtlasGeneration                = 0;
    cubeModelMatrices.reserve(_entityComponents.size() * 2);
    textVoxelModelMatrices.reserve(_entityComponents.size() * 2);
    panelModelMatrices.reserve(_entityComponents.size());
    glyphModelMatrices.reserve(_entityComponents.size());
    sphereModelMatrices.reserve(_entityComponents.size());

    for (auto &pair : _entityComponents)
    {
        if (pair.second)
        {
            switch (pair.second->getShaderProfile())
            {
            case RenderComponent::ShaderProfile::Cube:
                pair.second->appendModelMatrices(cubeModelMatrices);
                break;
            case RenderComponent::ShaderProfile::TextVoxel:
                pair.second->appendModelMatrices(textVoxelModelMatrices);
                break;
            case RenderComponent::ShaderProfile::Panel:
                pair.second->appendModelMatrices(panelModelMatrices);
                break;
            case RenderComponent::ShaderProfile::Glyph:
                pair.second->appendModelMatrices(glyphModelMatrices);
                if (collectSdf)
                {
                    pair.second->appendGlyphQuadVertices(sdfGlyphVertices);
                    if (!sdfAtlasPixels)
                    {
                        sdfAtlasPixels     = pair.second->getSdfAtlasPixels();
                        sdfAtlasGeneration = pair.second->getSdfAtlasGeneration();
                    }
                }
                break;
            case RenderComponent::ShaderProfile::Sphere:
                pair.second->appendModelMatrices(sphereModelMatrices);
                break;
            }
        }
    }

    if (_vulkanAPI)
    {
        _vulkanAPI->setEntityModelMatrices(
            std::move(cubeModelMatrices), std::move(textVoxelModelMatrices),
            std::move(panelModelMatrices), std::move(glyphModelMatrices),
            std::move(sphereModelMatrices));

        if (collectSdf)
        {
            _vulkanAPI->setSdfGlyphData(std::move(sdfGlyphVertices), sdfAtlasPixels,
                                        sdfAtlasGeneration);
            _glyphDirty = false;
        }
    }

    if (_vulkanAPI && _vulkanAPI->hasSwapchain())
        static_cast<void>(_vulkanAPI->drawFrame());

    for (auto &pair : _entityComponents)
    {
        if (pair.second)
        {
            pair.second->render();
        }
    }
}

bool RenderSystem::initializeWindowSurface(void *nativeWindowHandle, uint32_t width,
                                           uint32_t height)
{
    if (!_vulkanAPI || !nativeWindowHandle)
        return false;

    if (!_vulkanAPI->createSurface(nativeWindowHandle))
        return false;

    return _vulkanAPI->createSwapchain(width, height);
}

bool RenderSystem::resize(uint32_t width, uint32_t height)
{
    if (!_vulkanAPI)
        return false;

    return _vulkanAPI->recreateSwapchain(width, height);
}

void RenderSystem::beginCameraDrag(int x, int y)
{
    if (_vulkanAPI)
        _vulkanAPI->beginCameraDrag(x, y);
}

void RenderSystem::updateCameraDrag(int x, int y)
{
    if (_vulkanAPI)
        _vulkanAPI->updateCameraDrag(x, y);
}

void RenderSystem::endCameraDrag()
{
    if (_vulkanAPI)
        _vulkanAPI->endCameraDrag();
}

void RenderSystem::zoomCamera(int delta)
{
    if (_vulkanAPI)
        _vulkanAPI->zoomCamera(delta);
}

void RenderSystem::setSdfClipY(float yMin, float yMax)
{
    if (_vulkanAPI)
        _vulkanAPI->setSdfClipY(yMin, yMax);
}

void RenderSystem::setMoveForwardActive(bool active)
{
    if (_vulkanAPI)
        _vulkanAPI->setMoveForwardActive(active);
}

void RenderSystem::setMoveBackwardActive(bool active)
{
    if (_vulkanAPI)
        _vulkanAPI->setMoveBackwardActive(active);
}

void RenderSystem::setMoveLeftActive(bool active)
{
    if (_vulkanAPI)
        _vulkanAPI->setMoveLeftActive(active);
}

void RenderSystem::setMoveRightActive(bool active)
{
    if (_vulkanAPI)
        _vulkanAPI->setMoveRightActive(active);
}

void RenderSystem::setMoveUpActive(bool active)
{
    if (_vulkanAPI)
        _vulkanAPI->setMoveUpActive(active);
}

void RenderSystem::setMoveDownActive(bool active)
{
    if (_vulkanAPI)
        _vulkanAPI->setMoveDownActive(active);
}

void RenderSystem::setSprintActive(bool active)
{
    if (_vulkanAPI)
        _vulkanAPI->setSprintActive(active);
}

glm::vec3 RenderSystem::cameraForwardDirection() const
{
    if (!_vulkanAPI)
        return glm::vec3(0.0f, 0.0f, -1.0f);

    return _vulkanAPI->cameraForwardDirection();
}

bool RenderSystem::screenPointToRay(int x, int y, glm::vec3 &rayOrigin,
                                    glm::vec3 &rayDirection) const
{
    if (!_vulkanAPI)
        return false;

    return _vulkanAPI->screenPointToRay(x, y, rayOrigin, rayDirection);
}

bool RenderSystem::screenPointToRayFromNearPlane(int x, int y, glm::vec3 &rayOrigin,
                                                 glm::vec3 &rayDirection) const
{
    if (!_vulkanAPI)
        return false;

    return _vulkanAPI->screenPointToRayFromNearPlane(x, y, rayOrigin, rayDirection);
}

bool RenderSystem::hitTextEditorPanel(int x, int y) const
{
    if (!_vulkanAPI)
        return false;

    return _vulkanAPI->hitTextEditorPanel(x, y);
}

uint64_t RenderSystem::lastRenderedVertexCount() const
{
    if (!_vulkanAPI)
        return 0;

    return _vulkanAPI->lastRenderedVertexCount();
}

vigine::Entity *RenderSystem::pickFirstIntersectedEntity(int x, int y) const
{
    if (!_vulkanAPI)
        return nullptr;

    glm::vec3 rayOrigin(0.0f);
    glm::vec3 rayDirection(0.0f);
    if (!_vulkanAPI->screenPointToRay(x, y, rayOrigin, rayDirection))
        return nullptr;

    vigine::Entity *picked = nullptr;
    float bestT            = (std::numeric_limits<float>::max)();

    for (const auto &pair : _entityComponents)
    {
        if (!pair.first || !pair.second)
            continue;

        if (!pair.second->isPickable())
            continue;

        glm::vec3 minBounds(0.0f);
        glm::vec3 maxBounds(0.0f);
        if (!buildEntityAabb(*pair.second, minBounds, maxBounds))
            continue;

        float t = 0.0f;
        if (intersectRayAabb(rayOrigin, rayDirection, minBounds, maxBounds, t))
        {
            if (t >= 0.0f && t < bestT)
            {
                bestT  = t;
                picked = pair.first;
            }
        }
    }

    return picked;
}

void RenderSystem::entityBound()
{
    auto *boundEntity     = getBoundEntity();
    _boundEntityComponent = nullptr;

    if (!boundEntity)
        return;

    auto it = _entityComponents.find(boundEntity);
    if (it != _entityComponents.end())
        _boundEntityComponent = it->second.get();
}

void RenderSystem::entityUnbound() { _boundEntityComponent = nullptr; }

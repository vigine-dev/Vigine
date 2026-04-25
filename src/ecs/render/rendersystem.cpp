#include "vigine/ecs/render/rendersystem.h"

#include "vigine/impl/ecs/entity.h"
#include "vigine/ecs/render/graphicshandles.h"
#include "vigine/ecs/render/rendercomponent.h"
#include "vigine/ecs/render/texturecomponent.h"
#include "vigine/ecs/render/vulkanapi.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>

static constexpr uint32_t kSdfAtlasSize = 1024;

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

    // Procedural mesh: use transform scale as half-extents (local coords span -1..+1)
    // so world half-extent = scale. Ensure minimum thickness so flat objects are hittable.
    const glm::vec3 center = transform.getPosition();
    const glm::vec3 scale  = transform.getScale();
    const float minExt     = 0.15f;
    const glm::vec3 half(scale.x < minExt ? minExt : scale.x, scale.y < minExt ? minExt : scale.y,
                         scale.z < minExt ? minExt : scale.z);
    minBounds = center - half;
    maxBounds = center + half;
    return true;
}
} // namespace

using namespace vigine::graphics;

VulkanAPI *RenderSystem::vulkanAPI() const
{
    return static_cast<VulkanAPI *>(_graphicsBackend.get());
}

RenderSystem::RenderSystem(const SystemName &name)
    : AbstractSystem(name), _graphicsBackend(std::make_unique<VulkanAPI>()),
      _boundEntityComponent(nullptr), _boundTextureComponent(nullptr)
{
    // Vulkan init moved to initialize() -- called explicitly by application
}

bool RenderSystem::initialize(void *nativeWindowHandle, uint32_t width, uint32_t height)
{
    if (!vulkanAPI())
        return false;

    if (!vulkanAPI()->initializeInstance())
    {
        std::cerr << "Failed to initialize Vulkan instance" << std::endl;
        return false;
    }

    if (!vulkanAPI()->selectPhysicalDevice())
    {
        std::cerr << "Failed to select physical device" << std::endl;
        return false;
    }

    if (!vulkanAPI()->createLogicalDevice())
    {
        std::cerr << "Failed to create logical device" << std::endl;
        return false;
    }

    if (nativeWindowHandle)
    {
        if (!vulkanAPI()->createSurface(nativeWindowHandle))
        {
            std::cerr << "Failed to create Vulkan surface" << std::endl;
            return false;
        }

        if (!vulkanAPI()->createSwapchain(width, height))
        {
            std::cerr << "Failed to create Vulkan swapchain" << std::endl;
            return false;
        }
    }

    return true;
}

RenderSystem::~RenderSystem()
{
    // Clean up SDF atlas GPU texture before destroying backend.
    if (_sdfAtlasTextureComponent && _sdfAtlasTextureComponent->hasGpuTexture() && vulkanAPI())
    {
        vulkanAPI()->destroyTexture(_sdfAtlasTextureComponent->textureHandle());
        _sdfAtlasTextureComponent->setTextureHandle({});
    }
    _sdfAtlasTextureComponent.reset();

    _entityComponents.clear();
    _graphicsBackend.reset();
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
        // Cleanup GPU buffers before destroying component
        if (it->second)
        {
            auto &mesh = it->second->getMesh();
            if (mesh.vertexBufferHandle().isValid())
            {
                _graphicsBackend->destroyBuffer(mesh.vertexBufferHandle());
                mesh.setVertexBufferHandle({});
            }
            if (mesh.indexBufferHandle().isValid())
            {
                _graphicsBackend->destroyBuffer(mesh.indexBufferHandle());
                mesh.setIndexBufferHandle({});
            }
        }

        if (_boundEntityComponent == it->second.get())
            _boundEntityComponent = nullptr;

        _entityComponents.erase(it);
    }
}

RenderComponent *RenderSystem::boundRenderComponent() const { return _boundEntityComponent; }

TextureComponent *RenderSystem::boundTextureComponent() const { return _boundTextureComponent; }

void RenderSystem::createTextureComponent(Entity *entity)
{
    if (!entity)
        return;

    if (_textureComponents.find(entity) != _textureComponents.end())
        return;

    auto textureComponent      = std::make_unique<TextureComponent>();
    _textureComponents[entity] = std::move(textureComponent);
}

void RenderSystem::destroyTextureComponent(Entity *entity)
{
    if (!entity)
        return;

    auto it = _textureComponents.find(entity);
    if (it != _textureComponents.end())
    {
        if (_boundTextureComponent == it->second.get())
            _boundTextureComponent = nullptr;

        _textureComponents.erase(it);
    }
}

void RenderSystem::uploadTextureToGpu(Entity *entity)
{
    if (!entity)
        return;

    auto it = _textureComponents.find(entity);
    if (it == _textureComponents.end())
        return;

    auto *textureComponent = it->second.get();
    if (!textureComponent)
        return;

    // Skip if already uploaded or no pixel data
    if (textureComponent->hasGpuTexture() || textureComponent->pixelData().empty())
        return;

    // Create texture descriptor from component
    TextureDesc desc;
    desc.width     = textureComponent->width();
    desc.height    = textureComponent->height();
    desc.format    = textureComponent->format();
    desc.minFilter = textureComponent->minFilter();
    desc.magFilter = textureComponent->magFilter();
    desc.wrapU     = textureComponent->wrapU();
    desc.wrapV     = textureComponent->wrapV();

    // Create GPU texture via VulkanAPI
    auto *api = vulkanAPI();
    if (!api)
        return;

    TextureHandle handle = api->createTexture(desc);

    // Upload pixel data to GPU
    api->uploadTexture(handle, textureComponent->pixelData().data(), textureComponent->width(),
                       textureComponent->height());

    // Store handle in component
    textureComponent->setTextureHandle(handle);

    // Create Vulkan descriptor set so the texture can be bound in drawFrame().
    api->createTextureDescriptorSet(handle);
}

vigine::SystemId RenderSystem::id() const { return "Render"; }

void RenderSystem::markGlyphDirty() { _glyphDirty = true; }

void RenderSystem::update()
{
    // Invalidate entity pipeline cache when the swapchain was recreated (new render pass).
    if (vulkanAPI())
    {
        const uint32_t gen = vulkanAPI()->swapchainGeneration();
        if (gen != _lastSwapchainGeneration)
        {
            _pipelineCache.invalidate(*_graphicsBackend);
            _lastSwapchainGeneration = gen;
        }
    }

    // Group entities by pipeline key using PipelineCache.
    // Key = pipeline handle; value = EntityDrawGroup accumulating model matrices.
    std::unordered_map<uint64_t, EntityDrawGroup> groupMap;

    const bool collectSdf = _glyphDirty;
    std::vector<GlyphQuadVertex> sdfGlyphVertices;
    const std::vector<uint8_t> *sdfAtlasPixels = nullptr;
    uint32_t sdfAtlasGeneration                = 0;

    for (auto &pair : _entityComponents)
    {
        if (!pair.second)
            continue;

        auto &rc     = *pair.second;
        auto &shader = rc.getShader();
        auto &mesh   = rc.getMesh();

        // Skip entities with no shader configured (e.g. texture-only entities).
        if (shader.getVertexShaderPath().empty() || shader.getFragmentShaderPath().empty())
            continue;

        // GPU buffer management: upload CPU mesh data if needed
        if (!mesh.isProceduralInShader() && mesh.getVertexCount() > 0)
        {
            // Create/update vertex buffer if dirty or not yet uploaded
            if (mesh.isDirty() || !mesh.vertexBufferHandle().isValid())
            {
                const auto &vertices  = mesh.getVertices();
                const size_t dataSize = vertices.size() * sizeof(Vertex);

                // Destroy old buffer if exists
                if (mesh.vertexBufferHandle().isValid())
                {
                    _graphicsBackend->destroyBuffer(mesh.vertexBufferHandle());
                }

                // Create new buffer
                BufferDesc bufDesc;
                bufDesc.size        = dataSize;
                bufDesc.usage       = BufferUsage::Vertex;
                bufDesc.memoryUsage = MemoryUsage::GpuOnly;

                BufferHandle handle = _graphicsBackend->createBuffer(bufDesc);
                if (handle.isValid())
                {
                    _graphicsBackend->uploadBuffer(handle, vertices.data(), dataSize);
                    mesh.setVertexBufferHandle(handle);
                    mesh.clearDirty();
                }
            }

            // Create/update index buffer if indices exist and dirty
            if (mesh.getIndexCount() > 0 && (mesh.isDirty() || !mesh.indexBufferHandle().isValid()))
            {
                const auto &indices   = mesh.getIndices();
                const size_t dataSize = indices.size() * sizeof(uint32_t);

                // Destroy old buffer if exists
                if (mesh.indexBufferHandle().isValid())
                {
                    _graphicsBackend->destroyBuffer(mesh.indexBufferHandle());
                }

                // Create new buffer
                BufferDesc bufDesc;
                bufDesc.size        = dataSize;
                bufDesc.usage       = BufferUsage::Index;
                bufDesc.memoryUsage = MemoryUsage::GpuOnly;

                BufferHandle handle = _graphicsBackend->createBuffer(bufDesc);
                if (handle.isValid())
                {
                    _graphicsBackend->uploadBuffer(handle, indices.data(), dataSize);
                    mesh.setIndexBufferHandle(handle);
                }
            }
        }

        // Collect SDF glyph data for text entities regardless of entity pipeline state.
        // The SDF rendering path uses its own pipeline (_sdfGlyphPipeline), independent
        // of the entity's draw pipeline.
        if (collectSdf && shader.useVoxelTextLayout() == false)
        {
            rc.appendGlyphQuadVertices(sdfGlyphVertices);
            if (!sdfAtlasPixels)
            {
                sdfAtlasPixels     = rc.getSdfAtlasPixels();
                sdfAtlasGeneration = rc.getSdfAtlasGeneration();
            }
        }

        // Obtain (or create) a pipeline for this shader config.
        PipelineHandle ph = _pipelineCache.getOrCreate(*_graphicsBackend, shader);
        if (!ph.isValid())
            continue;

        // Determine vertex count from MeshComponent:
        // - If procedural in shader: use proceduralVertexCount()
        // - Otherwise: use actual vertex count from CPU data
        uint32_t vertexCount = 0;
        if (mesh.isProceduralInShader())
        {
            vertexCount = mesh.proceduralVertexCount();
        } else
        {
            vertexCount = mesh.getVertexCount();
        }

        // Skip if no geometry defined
        if (vertexCount == 0)
            continue;

        // Entities with different textures must not share a draw group (different descriptor sets).
        const TextureHandle textureHandle = rc.textureHandle();
        const uint64_t groupKey           = textureHandle.isValid()
                                                ? (ph.value ^ (textureHandle.value * 0x9e3779b97f4a7c15ULL))
                                                : ph.value;

        auto &group                       = groupMap[groupKey];
        group.pipeline                    = ph;
        group.textureHandle               = textureHandle;
        group.proceduralVertexCount       = vertexCount;
        group.instancedRendering          = shader.instancedRendering();
        if (_billboardEnabled && rc.getTransform().isBillboard())
        {
            const glm::mat4 bbMatrix =
                rc.getTransform().getBillboardModelMatrix(_camera.position());
            rc.appendModelMatrices(group.modelMatrices, bbMatrix);
        } else
        {
            rc.appendModelMatrices(group.modelMatrices);
        }
    }

    if (vulkanAPI())
    {
        std::vector<EntityDrawGroup> groups;
        groups.reserve(groupMap.size());
        for (auto &kv : groupMap)
            groups.push_back(std::move(kv.second));
        vulkanAPI()->setEntityDrawGroups(std::move(groups));

        if (collectSdf)
        {
            // Manage SDF atlas GPU upload via TextureComponent.
            // Upload to GPU only when atlas generation advances (new glyphs rasterized).
            if (sdfAtlasPixels && sdfAtlasGeneration != _sdfAtlasTrackedGeneration)
            {
                _sdfAtlasTrackedGeneration = sdfAtlasGeneration;
                auto *api                  = vulkanAPI();
                if (api)
                {
                    if (!_sdfAtlasTextureComponent)
                    {
                        _sdfAtlasTextureComponent = std::make_unique<TextureComponent>(
                            kSdfAtlasSize, kSdfAtlasSize, TextureFormat::R8_UNORM);
                        _sdfAtlasTextureComponent->setFilterMode(TextureFilter::Linear,
                                                                 TextureFilter::Linear);
                        _sdfAtlasTextureComponent->setWrapMode(TextureWrapMode::ClampToEdge,
                                                               TextureWrapMode::ClampToEdge);
                    }

                    if (!_sdfAtlasTextureComponent->hasGpuTexture())
                    {
                        TextureDesc desc;
                        desc.width                 = kSdfAtlasSize;
                        desc.height                = kSdfAtlasSize;
                        desc.format                = TextureFormat::R8_UNORM;
                        desc.minFilter             = TextureFilter::Linear;
                        desc.magFilter             = TextureFilter::Linear;
                        desc.wrapU                 = TextureWrapMode::ClampToEdge;
                        desc.wrapV                 = TextureWrapMode::ClampToEdge;
                        const TextureHandle handle = api->createTexture(desc);
                        _sdfAtlasTextureComponent->setTextureHandle(handle);
                    }

                    api->uploadTexture(_sdfAtlasTextureComponent->textureHandle(),
                                       sdfAtlasPixels->data(), kSdfAtlasSize, kSdfAtlasSize);
                }
            }

            const TextureHandle atlasHandle = _sdfAtlasTextureComponent
                                                  ? _sdfAtlasTextureComponent->textureHandle()
                                                  : TextureHandle{};
            vulkanAPI()->setSdfGlyphData(std::move(sdfGlyphVertices), atlasHandle);
            _glyphDirty = false;
        }
    }

    // Update camera physics
    const auto now = std::chrono::steady_clock::now();
    static std::chrono::steady_clock::time_point lastFrameTime;
    float deltaSeconds = 0.0f;
    if (lastFrameTime.time_since_epoch().count() != 0)
    {
        deltaSeconds = std::chrono::duration<float>(now - lastFrameTime).count();
    }
    lastFrameTime = now;
    _camera.update(deltaSeconds);

    // Calculate view-projection matrix
    const float aspect = static_cast<float>(vulkanAPI()->swapchainWidth()) /
                         static_cast<float>((std::max)(vulkanAPI()->swapchainHeight(), 1u));
    const glm::mat4 viewProjection = _camera.viewProjectionMatrix(aspect);

    if (_graphicsBackend && vulkanAPI()->hasSwapchain())
        static_cast<void>(vulkanAPI()->drawFrame(viewProjection));

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
    if (!vulkanAPI() || !nativeWindowHandle)
        return false;

    if (!vulkanAPI()->createSurface(nativeWindowHandle))
        return false;

    return vulkanAPI()->createSwapchain(width, height);
}

bool RenderSystem::resize(uint32_t width, uint32_t height)
{
    if (!vulkanAPI())
        return false;

    return vulkanAPI()->recreateSwapchain(width, height);
}

void RenderSystem::beginCameraDrag(int x, int y) { _camera.beginDrag(x, y); }

void RenderSystem::updateCameraDrag(int x, int y) { _camera.updateDrag(x, y); }

void RenderSystem::endCameraDrag() { _camera.endDrag(); }

void RenderSystem::zoomCamera(int delta) { _camera.zoom(delta); }

void RenderSystem::setSdfClipY(float yMin, float yMax)
{
    if (vulkanAPI())
        vulkanAPI()->setSdfClipY(yMin, yMax);
}

void RenderSystem::setMoveForwardActive(bool active) { _camera.setMoveForwardActive(active); }

void RenderSystem::setMoveBackwardActive(bool active) { _camera.setMoveBackwardActive(active); }

void RenderSystem::setMoveLeftActive(bool active) { _camera.setMoveLeftActive(active); }

void RenderSystem::setMoveRightActive(bool active) { _camera.setMoveRightActive(active); }

void RenderSystem::setMoveUpActive(bool active) { _camera.setMoveUpActive(active); }

void RenderSystem::setMoveDownActive(bool active) { _camera.setMoveDownActive(active); }

void RenderSystem::setSprintActive(bool active) { _camera.setSprintActive(active); }

void RenderSystem::setBillboardEnabled(bool enabled) { _billboardEnabled = enabled; }

bool RenderSystem::isBillboardEnabled() const { return _billboardEnabled; }

void RenderSystem::toggleBillboard()
{
    _billboardEnabled = !_billboardEnabled;
    std::cout << "[RenderSystem] Billboard " << (_billboardEnabled ? "enabled" : "disabled")
              << std::endl;
}

glm::vec3 RenderSystem::cameraForwardDirection() const { return _camera.forward(); }

bool RenderSystem::screenPointToRay(int x, int y, glm::vec3 &rayOrigin,
                                    glm::vec3 &rayDirection) const
{
    if (!vulkanAPI())
        return false;

    return _camera.screenPointToRay(x, y, vulkanAPI()->swapchainWidth(),
                                    vulkanAPI()->swapchainHeight(), rayOrigin, rayDirection);
}

bool RenderSystem::screenPointToRayFromNearPlane(int x, int y, glm::vec3 &rayOrigin,
                                                 glm::vec3 &rayDirection) const
{
    if (!vulkanAPI())
        return false;

    return _camera.screenPointToRayFromNearPlane(x, y, vulkanAPI()->swapchainWidth(),
                                                 vulkanAPI()->swapchainHeight(), rayOrigin,
                                                 rayDirection);
}

bool RenderSystem::hitTextEditorPanel(int x, int y) const
{
    glm::vec3 rayOrigin(0.0f);
    glm::vec3 rayDirection(0.0f);
    if (!screenPointToRay(x, y, rayOrigin, rayDirection))
        return false;

    // Editor board is a plane centered at (0, 1.65, 1.2) with size 4.8 x 1.5 in XY.
    if (std::abs(rayDirection.z) < 1e-6f)
        return false;

    const float t = (1.2f - rayOrigin.z) / rayDirection.z;
    if (t <= 0.0f)
        return false;

    const glm::vec3 hitPoint   = rayOrigin + rayDirection * t;
    constexpr float halfWidth  = 2.4f;
    constexpr float halfHeight = 0.75f;
    constexpr float centerY    = 1.65f;

    return hitPoint.x >= -halfWidth && hitPoint.x <= halfWidth &&
           hitPoint.y >= (centerY - halfHeight) && hitPoint.y <= (centerY + halfHeight);
}

uint64_t RenderSystem::lastRenderedVertexCount() const
{
    if (!vulkanAPI())
        return 0;

    return vulkanAPI()->lastRenderedVertexCount();
}

vigine::Entity *RenderSystem::pickFirstIntersectedEntity(int x, int y) const
{
    if (!vulkanAPI())
        return nullptr;

    glm::vec3 rayOrigin(0.0f);
    glm::vec3 rayDirection(0.0f);
    if (!screenPointToRay(x, y, rayOrigin, rayDirection))
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
    auto *boundEntity      = getBoundEntity();
    _boundEntityComponent  = nullptr;
    _boundTextureComponent = nullptr;

    if (!boundEntity)
        return;

    auto it = _entityComponents.find(boundEntity);
    if (it != _entityComponents.end())
        _boundEntityComponent = it->second.get();

    auto texIt = _textureComponents.find(boundEntity);
    if (texIt != _textureComponents.end())
        _boundTextureComponent = texIt->second.get();
}

void RenderSystem::entityUnbound()
{
    _boundEntityComponent  = nullptr;
    _boundTextureComponent = nullptr;
}

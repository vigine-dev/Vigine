#pragma once

#include "vigine/base/macros.h"
#include "vigine/ecs/abstractsystem.h"
#include "vigine/ecs/render/camera.h"
#include "vigine/ecs/render/pipelinecache.h"
#include "vigine/ecs/render/textcomponent.h"

#include <cstdint>
#include <glm/vec3.hpp>
#include <memory>
#include <unordered_map>

namespace vigine
{
namespace graphics
{

class RenderComponent;
class TextureComponent;
class GraphicsBackend;

// TODO: create skeleton
class RenderSystem : public AbstractSystem
{
  public:
    RenderSystem(const SystemName &name);
    ~RenderSystem() override;

    [[nodiscard]] SystemId id() const override;

    // interface implementation
    [[nodiscard]] bool hasComponents(Entity *entity) const override;
    void createComponents(Entity *entity) override;
    void destroyComponents(Entity *entity) override;
    RenderComponent *boundRenderComponent() const;
    TextureComponent *boundTextureComponent() const;
    void createTextureComponent(Entity *entity);
    void destroyTextureComponent(Entity *entity);
    void uploadTextureToGpu(Entity *entity);

    void update();
    void markGlyphDirty();
    [[nodiscard]] bool initialize(void *nativeWindowHandle, uint32_t width, uint32_t height);
    [[nodiscard]] bool initializeWindowSurface(void *nativeWindowHandle, uint32_t width,
                                               uint32_t height);
    [[nodiscard]] bool resize(uint32_t width, uint32_t height);
    void beginCameraDrag(int x, int y);
    void updateCameraDrag(int x, int y);
    void endCameraDrag();
    void zoomCamera(int delta);
    void setSdfClipY(float yMin, float yMax);
    void setMoveForwardActive(bool active);
    void setMoveBackwardActive(bool active);
    void setMoveLeftActive(bool active);
    void setMoveRightActive(bool active);
    void setMoveUpActive(bool active);
    void setMoveDownActive(bool active);
    void setSprintActive(bool active);
    void setBillboardEnabled(bool enabled);
    bool isBillboardEnabled() const;
    void toggleBillboard();
    [[nodiscard]] glm::vec3 cameraForwardDirection() const;
    [[nodiscard]] bool screenPointToRay(int x, int y, glm::vec3 &rayOrigin,
                                        glm::vec3 &rayDirection) const;
    [[nodiscard]] bool screenPointToRayFromNearPlane(int x, int y, glm::vec3 &rayOrigin,
                                                     glm::vec3 &rayDirection) const;
    [[nodiscard]] vigine::Entity *pickFirstIntersectedEntity(int x, int y) const;
    [[nodiscard]] bool hitTextEditorPanel(int x, int y) const;
    [[nodiscard]] uint64_t lastRenderedVertexCount() const;

  protected:
    void entityBound() override;
    void entityUnbound() override;

  private:
    // Helper to access VulkanAPI-specific methods until Phase 2-6 refactoring complete
    class VulkanAPI *vulkanAPI() const;

    std::unique_ptr<GraphicsBackend> _graphicsBackend;
    PipelineCache _pipelineCache;
    Camera _camera;
    std::unordered_map<Entity *, std::unique_ptr<RenderComponent>> _entityComponents;
    std::unordered_map<Entity *, std::unique_ptr<TextureComponent>> _textureComponents;
    std::unique_ptr<TextureComponent> _sdfAtlasTextureComponent;
    uint32_t _sdfAtlasTrackedGeneration{0};
    RenderComponent *_boundEntityComponent;
    TextureComponent *_boundTextureComponent;
    bool _glyphDirty{true};
    bool _billboardEnabled{true};
    uint32_t _lastSwapchainGeneration{0};
};

BUILD_SMART_PTR(RenderSystem);

} // namespace graphics
} // namespace vigine

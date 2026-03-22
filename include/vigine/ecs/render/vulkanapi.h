#pragma once

#include "vigine/base/macros.h"
#include "vigine/ecs/render/textcomponent.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace vigine
{
namespace graphics
{

class VulkanAPI
{
  public:
    VulkanAPI();
    ~VulkanAPI();

    // Initialization
    bool initializeInstance();
    bool selectPhysicalDevice();
    bool createLogicalDevice();
    bool createSurface(void *nativeWindowHandle);
    bool createSwapchain(uint32_t width, uint32_t height);
    bool recreateSwapchain(uint32_t width, uint32_t height);
    bool drawFrame();
    void setEntityModelMatrices(std::vector<glm::mat4> cubeMatrices,
                                std::vector<glm::mat4> textVoxelMatrices,
                                std::vector<glm::mat4> panelMatrices,
                                std::vector<glm::mat4> glyphMatrices,
                                std::vector<glm::mat4> sphereMatrices);
    // SDF glyph rendering: flat vertex list + atlas upload.
    void setSdfGlyphData(std::vector<GlyphQuadVertex> vertices,
                         const std::vector<uint8_t> *atlasPixels, uint32_t atlasGeneration);
    void beginCameraDrag(int x, int y);
    void updateCameraDrag(int x, int y);
    void endCameraDrag();
    void zoomCamera(int delta);
    void setMoveForwardActive(bool active);
    void setMoveBackwardActive(bool active);
    void setMoveLeftActive(bool active);
    void setMoveRightActive(bool active);
    void setMoveUpActive(bool active);
    void setMoveDownActive(bool active);
    void setSprintActive(bool active);
    glm::vec3 cameraForwardDirection() const;
    bool screenPointToRay(int x, int y, glm::vec3 &rayOrigin, glm::vec3 &rayDirection) const;
    bool screenPointToRayFromNearPlane(int x, int y, glm::vec3 &rayOrigin,
                                       glm::vec3 &rayDirection) const;
    bool hitTextEditorPanel(int x, int y) const;
    [[nodiscard]] uint64_t lastRenderedVertexCount() const { return _lastRenderedVertexCount; }

    // SDF text clip planes (world Y). clipYMax == 0 → no clipping.
    void setSdfClipY(float yMin, float yMax)
    {
        _sdfClipYMin = yMin;
        _sdfClipYMax = yMax;
    }

    // Vulkan resources access
    vk::Instance getInstance() const { return _instance.get(); }
    vk::PhysicalDevice getPhysicalDevice() const { return _physicalDevice; }
    vk::Device getLogicalDevice() const { return _device.get(); }
    vk::Queue getGraphicsQueue() const { return _graphicsQueue; }

    // Utility
    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
    bool isInitialized() const { return _initialized; }
    bool hasSwapchain() const { return static_cast<bool>(_swapchain); }

  private:
    struct PushConstants
    {
        glm::mat4 viewProjection{1.0f};
        glm::vec4 animationData{0.0f};
        glm::vec4 sunDirectionIntensity{0.0f};
        glm::vec4 lightingParams{0.0f};
        glm::mat4 modelMatrix{1.0f};
    };

    void cleanupSwapchainResources();
    bool recreateSwapchainFromSurfaceExtent();

    bool _initialized{false};
    float _sdfClipYMin{0.f};
    float _sdfClipYMax{0.f};

    // Vulkan objects
    vk::UniqueInstance _instance;
    vk::PhysicalDevice _physicalDevice;
    vk::UniqueDevice _device;
    vk::SurfaceKHR _surface;
    vk::UniqueSwapchainKHR _swapchain;
    vk::Queue _graphicsQueue;
    vk::Queue _presentQueue;

    vk::Format _swapchainFormat{vk::Format::eUndefined};
    vk::Format _depthFormat{vk::Format::eUndefined};
    vk::Extent2D _swapchainExtent{};
    std::vector<vk::UniqueSemaphore> _imageAvailableSemaphores;
    std::vector<vk::UniqueSemaphore> _renderFinishedSemaphores;
    std::vector<vk::UniqueFence> _inFlightFences;
    std::vector<vk::Fence> _imagesInFlight;
    std::size_t _currentFrame{0};
    vk::UniqueCommandPool _commandPool;
    std::vector<vk::UniqueImageView> _swapchainImageViews;
    vk::UniqueImage _depthImage;
    vk::UniqueDeviceMemory _depthImageMemory;
    vk::UniqueImageView _depthImageView;
    std::vector<vk::UniqueFramebuffer> _swapchainFramebuffers;
    vk::UniqueRenderPass _renderPass;
    vk::UniquePipelineLayout _pipelineLayout;
    vk::UniquePipeline _graphicsPipeline;
    vk::UniquePipeline _textVoxelPipeline;
    vk::UniquePipeline _panelPipeline;
    vk::UniquePipeline _glyphPipeline;
    vk::UniquePipeline _spherePipeline;
    vk::UniquePipeline _pyramidPipeline;
    vk::UniquePipeline _gridPipeline;
    vk::UniquePipeline _sunPipeline;
    std::vector<vk::Image> _swapchainImages;
    std::vector<vk::CommandBuffer> _commandBuffers;
    std::vector<uint8_t> _imageInitialized;
    float _cubeRotationAngle{0.0f};
    float _pyramidRotationAngle{0.0f};
    std::vector<glm::mat4> _cubeEntityModelMatrices;
    std::vector<glm::mat4> _textVoxelEntityModelMatrices;
    std::vector<glm::mat4> _panelEntityModelMatrices;
    std::vector<glm::mat4> _glyphEntityModelMatrices;
    std::vector<glm::mat4> _sphereEntityModelMatrices;

    // Per-swapchain-image instance buffers for glyph instanced rendering.
    std::vector<vk::UniqueBuffer> _glyphInstanceBuffers;
    std::vector<vk::UniqueDeviceMemory> _glyphInstanceMemories;
    std::vector<vk::DeviceSize> _glyphInstanceBufferCapacities;
    std::vector<bool> _glyphInstanceUploadNeeded;

    // SDF glyph flat vertex buffer (one buffer per swapchain image).
    std::vector<GlyphQuadVertex> _sdfGlyphVertices;
    std::vector<bool> _sdfGlyphUploadNeeded;
    std::vector<vk::UniqueBuffer> _sdfGlyphVertexBuffers;
    std::vector<vk::UniqueDeviceMemory> _sdfGlyphVertexMemories;
    std::vector<vk::DeviceSize> _sdfGlyphVertexCapacities;

    // SDF font atlas Vulkan texture (one global atlas, recreated on font change).
    vk::UniqueImage _sdfAtlasImage;
    vk::UniqueDeviceMemory _sdfAtlasMemory;
    vk::UniqueImageView _sdfAtlasImageView;
    vk::UniqueSampler _sdfAtlasSampler;
    vk::UniqueDescriptorSetLayout _sdfDescriptorSetLayout;
    vk::UniqueDescriptorPool _sdfDescriptorPool;
    vk::DescriptorSet _sdfDescriptorSet;
    vk::UniquePipelineLayout _sdfPipelineLayout;
    vk::UniquePipeline _sdfGlyphPipeline;
    bool _sdfAtlasReady{false};
    uint32_t _sdfAtlasGeneration{0};
    float _cameraYaw{0.0f};
    float _cameraPitch{-0.2f};
    glm::vec3 _cameraPosition{0.0f, 1.6f, 4.5f};
    bool _moveForwardActive{false};
    bool _moveBackwardActive{false};
    bool _moveLeftActive{false};
    bool _moveRightActive{false};
    bool _moveUpActive{false};
    bool _moveDownActive{false};
    bool _sprintActive{false};
    glm::vec3 _cameraVelocity{0.0f, 0.0f, 0.0f};
    bool _cameraDragActive{false};
    int _lastCameraPointerX{0};
    int _lastCameraPointerY{0};
    std::chrono::steady_clock::time_point _lastFrameTime{};
    bool _swapchainRecreateRequested{false};
    uint64_t _lastRenderedVertexCount{0};

    uint32_t _graphicsQueueFamily{0};
    uint32_t _presentQueueFamily{0};

    // Validation layers
    std::vector<const char *> _validationLayers;
    bool _enableValidationLayers{true};

    static constexpr std::size_t kMaxFramesInFlight = 2;
};

BUILD_SMART_PTR(VulkanAPI);

} // namespace graphics
} // namespace vigine

#pragma once

#include "vigine/base/macros.h"

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
    };

    void cleanupSwapchainResources();
    bool recreateSwapchainFromSurfaceExtent();

    bool _initialized{false};

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
    vk::UniquePipeline _pyramidPipeline;
    vk::UniquePipeline _gridPipeline;
    std::vector<vk::Image> _swapchainImages;
    std::vector<vk::CommandBuffer> _commandBuffers;
    std::vector<uint8_t> _imageInitialized;
    float _cubeRotationAngle{0.0f};
    float _pyramidRotationAngle{0.0f};
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

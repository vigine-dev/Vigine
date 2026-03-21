#pragma once

#include "vigine/base/macros.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
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
    std::vector<vk::Image> _swapchainImages;
    std::vector<vk::CommandBuffer> _commandBuffers;
    std::vector<uint8_t> _imageInitialized;
    float _cubeRotationAngle{0.0f};
    float _pyramidRotationAngle{0.0f};
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

#pragma once

#include "surfacefactory.h"

#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace vigine
{
namespace graphics
{

class VulkanDevice
{
  public:
    VulkanDevice();
    ~VulkanDevice();

    bool initializeInstance();
    bool selectPhysicalDevice();
    bool createLogicalDevice();
    bool createSurface(void *nativeWindowHandle);

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

    vk::Instance instance() const { return _instance.get(); }
    vk::PhysicalDevice physicalDevice() const { return _physicalDevice; }
    vk::Device device() const { return _device.get(); }
    vk::SurfaceKHR surface() const { return _surface; }
    vk::Queue graphicsQueue() const { return _graphicsQueue; }
    vk::Queue presentQueue() const { return _presentQueue; }
    uint32_t graphicsQueueFamily() const { return _graphicsQueueFamily; }
    uint32_t presentQueueFamily() const { return _presentQueueFamily; }
    bool isInitialized() const { return _initialized; }

  private:
    vk::UniqueInstance _instance;
    vk::PhysicalDevice _physicalDevice;
    vk::UniqueDevice _device;
    vk::SurfaceKHR _surface;
    vk::Queue _graphicsQueue;
    vk::Queue _presentQueue;
    uint32_t _graphicsQueueFamily{0};
    uint32_t _presentQueueFamily{0};
    bool _initialized{false};

    std::unique_ptr<SurfaceFactory> _surfaceFactory;
    std::vector<const char *> _validationLayers;
    bool _enableValidationLayers{true};
};

} // namespace graphics
} // namespace vigine

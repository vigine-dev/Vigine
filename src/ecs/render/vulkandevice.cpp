#include "vulkandevice.h"

#include <iostream>
#include <stdexcept>
#include <vector>

#ifdef __APPLE__
#include <vulkan/vulkan_metal.h>
#endif

using namespace vigine::graphics;

VulkanDevice::VulkanDevice()
{
    _validationLayers.push_back("VK_LAYER_KHRONOS_validation");
    _surfaceFactory = getPlatformSurfaceFactory();
}

VulkanDevice::~VulkanDevice()
{
    if (_instance && _surface)
        _instance->destroySurfaceKHR(_surface);
}

bool VulkanDevice::initializeInstance()
{
    if (_initialized)
        return true;

    if (!_surfaceFactory)
    {
        std::cerr << "No surface factory available for this platform" << std::endl;
        return false;
    }

    // Check available validation layers
    auto availableLayers = vk::enumerateInstanceLayerProperties();

    // Create instance
    vk::ApplicationInfo appInfo;
    appInfo.pApplicationName   = "Vigine Example Window";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "Vigine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_0;

    // Get platform-specific extensions from factory
    std::vector<const char *> extensions = _surfaceFactory->requiredInstanceExtensions();

    vk::InstanceCreateInfo createInfo;
    createInfo.flags                 = _surfaceFactory->instanceCreateFlags();
    createInfo.pApplicationInfo      = &appInfo;
    createInfo.enabledLayerCount     = _enableValidationLayers ? 1u : 0u;
    createInfo.ppEnabledLayerNames   = _enableValidationLayers ? _validationLayers.data() : nullptr;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    try
    {
        _instance = vk::createInstanceUnique(createInfo);
        std::cout << "Vulkan instance created successfully" << std::endl;
        return true;
    } catch (const std::exception &e)
    {
        std::cerr << "Failed to create Vulkan instance: " << e.what() << std::endl;
        return false;
    }
}

bool VulkanDevice::selectPhysicalDevice()
{
    if (!_instance)
        return false;

    try
    {
        auto physicalDevices = _instance->enumeratePhysicalDevices();
        if (physicalDevices.empty())
        {
            std::cerr << "No Vulkan physical devices found" << std::endl;
            return false;
        }

        _physicalDevice = physicalDevices[0];
        auto properties = _physicalDevice.getProperties();
        std::cout << "Selected GPU: " << properties.deviceName << std::endl;

        // Find queue families
        auto queueFamilies = _physicalDevice.getQueueFamilyProperties();
        for (uint32_t i = 0; i < queueFamilies.size(); ++i)
        {
            if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics)
            {
                _graphicsQueueFamily = i;
                _presentQueueFamily  = i; // For simplicity, use the same queue
            }
        }

        return true;
    } catch (const std::exception &e)
    {
        std::cerr << "Failed to select physical device: " << e.what() << std::endl;
        return false;
    }
}

bool VulkanDevice::createLogicalDevice()
{
    if (!_physicalDevice)
        return false;

    try
    {
        float queuePriority = 1.0f;

        vk::DeviceQueueCreateInfo queueCreateInfo;
        queueCreateInfo.queueFamilyIndex = _graphicsQueueFamily;
        queueCreateInfo.queueCount       = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        vk::PhysicalDeviceFeatures deviceFeatures;

        std::vector<const char *> extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };

#ifdef __APPLE__
        extensions.push_back("VK_KHR_portability_subset");
#endif

        vk::DeviceCreateInfo createInfo;
        createInfo.queueCreateInfoCount    = 1;
        createInfo.pQueueCreateInfos       = &queueCreateInfo;
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        createInfo.pEnabledFeatures        = &deviceFeatures;

        _device                            = _physicalDevice.createDeviceUnique(createInfo);
        _graphicsQueue                     = _device->getQueue(_graphicsQueueFamily, 0);
        _presentQueue                      = _device->getQueue(_presentQueueFamily, 0);

        std::cout << "Logical device created successfully" << std::endl;
        _initialized = true;
        return true;
    } catch (const std::exception &e)
    {
        std::cerr << "Failed to create logical device: " << e.what() << std::endl;
        return false;
    }
}

bool VulkanDevice::createSurface(void *nativeWindowHandle)
{
    if (!_instance || !nativeWindowHandle || !_surfaceFactory)
        return false;

    try
    {
        if (_surface)
        {
            _instance->destroySurfaceKHR(_surface);
            _surface = vk::SurfaceKHR{};
        }

        _surface = _surfaceFactory->createSurface(_instance.get(), nativeWindowHandle);
        if (!_surface)
        {
            std::cerr << "Failed to create surface via factory" << std::endl;
            return false;
        }

        const bool supportsPresent =
            _physicalDevice.getSurfaceSupportKHR(_presentQueueFamily, _surface);
        if (!supportsPresent)
        {
            std::cerr << "Selected queue family does not support present" << std::endl;
            return false;
        }

        return true;
    } catch (const std::exception &e)
    {
        std::cerr << "Failed to create Vulkan surface: " << e.what() << std::endl;
        return false;
    }
}

uint32_t VulkanDevice::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
    auto memProperties = _physicalDevice.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

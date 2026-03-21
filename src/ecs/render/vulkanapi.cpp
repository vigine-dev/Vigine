#include "vigine/ecs/render/vulkanapi.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <vulkan/vulkan_win32.h>
#include <windows.h>

#endif

// Define Windows surface extensions
#ifndef VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#define VK_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_win32_surface"
#endif

using namespace vigine::graphics;

namespace
{
struct CameraControlsConfig
{
    float mouseLookSensitivity{0.01f};
    float pitchLimitRad{1.45f};
    float moveSpeedUnitsPerSec{3.5f};
    float sprintMultiplier{2.1f};
    float wheelStep{120.0f};
    float wheelMovePerStep{0.8f};
    float acceleration{12.0f};
    float stopVelocityEpsilon{0.01f};
    float nearPlane{0.1f};
    float farPlane{250.0f};
};

constexpr CameraControlsConfig kCameraControls{};

glm::vec3 cameraForward(float yaw, float pitch)
{
    return glm::normalize(glm::vec3(std::cos(pitch) * std::sin(yaw), std::sin(pitch),
                                    -std::cos(pitch) * std::cos(yaw)));
}

glm::vec3 flatForward(float yaw)
{
    return glm::normalize(glm::vec3(std::sin(yaw), 0.0f, -std::cos(yaw)));
}

std::vector<char> loadBinaryFile(const std::vector<std::string> &candidates)
{
    for (const auto &path : candidates)
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open())
            continue;

        const std::streamsize fileSize = file.tellg();
        if (fileSize <= 0)
            continue;

        std::vector<char> buffer(static_cast<size_t>(fileSize));
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        return buffer;
    }

    return {};
}
} // namespace

VulkanAPI::VulkanAPI() { _validationLayers.push_back("VK_LAYER_KHRONOS_validation"); }

void VulkanAPI::beginCameraDrag(int x, int y)
{
    _cameraDragActive   = true;
    _lastCameraPointerX = x;
    _lastCameraPointerY = y;
}

void VulkanAPI::updateCameraDrag(int x, int y)
{
    if (!_cameraDragActive)
        return;

    const int deltaX  = x - _lastCameraPointerX;
    const int deltaY  = y - _lastCameraPointerY;

    _cameraYaw       -= static_cast<float>(deltaX) * kCameraControls.mouseLookSensitivity;
    _cameraPitch =
        std::clamp(_cameraPitch + static_cast<float>(deltaY) * kCameraControls.mouseLookSensitivity,
                   -kCameraControls.pitchLimitRad, kCameraControls.pitchLimitRad);

    _lastCameraPointerX = x;
    _lastCameraPointerY = y;
}

void VulkanAPI::endCameraDrag() { _cameraDragActive = false; }

void VulkanAPI::zoomCamera(int delta)
{
    _cameraPosition += cameraForward(_cameraYaw, _cameraPitch) *
                       ((static_cast<float>(delta) / kCameraControls.wheelStep) *
                        kCameraControls.wheelMovePerStep);
}

void VulkanAPI::setMoveForwardActive(bool active) { _moveForwardActive = active; }

void VulkanAPI::setMoveBackwardActive(bool active) { _moveBackwardActive = active; }

void VulkanAPI::setMoveLeftActive(bool active) { _moveLeftActive = active; }

void VulkanAPI::setMoveRightActive(bool active) { _moveRightActive = active; }

void VulkanAPI::setMoveUpActive(bool active) { _moveUpActive = active; }

void VulkanAPI::setMoveDownActive(bool active) { _moveDownActive = active; }

void VulkanAPI::setSprintActive(bool active) { _sprintActive = active; }

VulkanAPI::~VulkanAPI()
{
    cleanupSwapchainResources();

    if (_instance && _surface)
        _instance->destroySurfaceKHR(_surface);
}

void VulkanAPI::cleanupSwapchainResources()
{
    if (!_device)
        return;

    static_cast<void>(_device->waitIdle());

    _commandBuffers.clear();
    _commandPool.reset();

    _inFlightFences.clear();
    _renderFinishedSemaphores.clear();
    _imageAvailableSemaphores.clear();
    _imagesInFlight.clear();
    _currentFrame = 0;

    _swapchainFramebuffers.clear();
    _sunPipeline.reset();
    _gridPipeline.reset();
    _pyramidPipeline.reset();
    _graphicsPipeline.reset();
    _pipelineLayout.reset();
    _renderPass.reset();

    _swapchainImageViews.clear();
    _depthImageView.reset();
    _depthImage.reset();
    _depthImageMemory.reset();

    _swapchainImages.clear();
    _imageInitialized.clear();
    _swapchain.reset();
    _swapchainRecreateRequested = false;
}

bool VulkanAPI::recreateSwapchainFromSurfaceExtent()
{
    if (!_surface || !_physicalDevice)
        return false;

    auto capabilities = _physicalDevice.getSurfaceCapabilitiesKHR(_surface);
    if (capabilities.currentExtent.width == 0 || capabilities.currentExtent.height == 0)
        return false;

    if (capabilities.currentExtent.width == (std::numeric_limits<uint32_t>::max)() ||
        capabilities.currentExtent.height == (std::numeric_limits<uint32_t>::max)())
    {
        if (_swapchainExtent.width == 0 || _swapchainExtent.height == 0)
            return false;

        return recreateSwapchain(_swapchainExtent.width, _swapchainExtent.height);
    }

    return recreateSwapchain(capabilities.currentExtent.width, capabilities.currentExtent.height);
}

bool VulkanAPI::initializeInstance()
{
    if (_initialized)
        return true;

    // Check available validation layers
    auto availableLayers = vk::enumerateInstanceLayerProperties();

    // Create instance
    vk::ApplicationInfo appInfo;
    appInfo.pApplicationName             = "Vigine Example Window";
    appInfo.applicationVersion           = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName                  = "Vigine";
    appInfo.engineVersion                = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion                   = VK_API_VERSION_1_0;

    std::vector<const char *> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
    };

#ifdef _WIN32
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif

    vk::InstanceCreateInfo createInfo;
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

bool VulkanAPI::selectPhysicalDevice()
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

bool VulkanAPI::createLogicalDevice()
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

bool VulkanAPI::createSurface(void *nativeWindowHandle)
{
    if (!_instance || !nativeWindowHandle)
        return false;

#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(nativeWindowHandle);
    if (!hwnd)
        return false;

    try
    {
        cleanupSwapchainResources();

        if (_surface)
        {
            _instance->destroySurfaceKHR(_surface);
            _surface = vk::SurfaceKHR{};
        }

        VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType            = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hwnd             = hwnd;
        createInfo.hinstance        = GetModuleHandleA(nullptr);

        VkSurfaceKHR rawSurface     = VK_NULL_HANDLE;
        const VkResult createResult = vkCreateWin32SurfaceKHR(
            static_cast<VkInstance>(_instance.get()), &createInfo, nullptr, &rawSurface);
        if (createResult != VK_SUCCESS)
        {
            std::cerr << "Failed to create Win32 surface, VkResult="
                      << static_cast<int>(createResult) << std::endl;
            return false;
        }

        _surface = vk::SurfaceKHR(rawSurface);

        const bool supportsPresent =
            _physicalDevice.getSurfaceSupportKHR(_presentQueueFamily, _surface);
        if (!supportsPresent)
        {
            std::cerr << "Selected queue family does not support present" << std::endl;
            return false;
        }

        std::cout << "Vulkan surface created successfully" << std::endl;
        return true;
    } catch (const std::exception &e)
    {
        std::cerr << "Failed to create Vulkan surface: " << e.what() << std::endl;
        return false;
    }
#else
    static_cast<void>(nativeWindowHandle);
    std::cerr << "Surface creation is not implemented for this platform" << std::endl;
    return false;
#endif
}

bool VulkanAPI::createSwapchain(uint32_t width, uint32_t height)
{
    if (!_device || !_surface)
        return false;

    try
    {
        cleanupSwapchainResources();

        auto capabilities = _physicalDevice.getSurfaceCapabilitiesKHR(_surface);
        auto formats      = _physicalDevice.getSurfaceFormatsKHR(_surface);
        auto presentModes = _physicalDevice.getSurfacePresentModesKHR(_surface);

        if (formats.empty() || presentModes.empty())
        {
            std::cerr << "No swapchain formats or present modes available" << std::endl;
            return false;
        }

        vk::SurfaceFormatKHR chosenFormat = formats[0];
        for (const auto &format : formats)
        {
            if (format.format == vk::Format::eB8G8R8A8Srgb &&
                format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                chosenFormat = format;
                break;
            }
        }

        vk::PresentModeKHR chosenPresentMode = vk::PresentModeKHR::eFifo;
        for (const auto &mode : presentModes)
        {
            if (mode == vk::PresentModeKHR::eMailbox)
            {
                chosenPresentMode = mode;
                break;
            }
        }

        vk::Extent2D extent;
        if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)())
        {
            extent = capabilities.currentExtent;
        } else
        {
            extent.width  = std::clamp(width, capabilities.minImageExtent.width,
                                       capabilities.maxImageExtent.width);
            extent.height = std::clamp(height, capabilities.minImageExtent.height,
                                       capabilities.maxImageExtent.height);
        }

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
            imageCount = capabilities.maxImageCount;

        vk::SwapchainCreateInfoKHR createInfo;
        createInfo.surface          = _surface;
        createInfo.minImageCount    = imageCount;
        createInfo.imageFormat      = chosenFormat.format;
        createInfo.imageColorSpace  = chosenFormat.colorSpace;
        createInfo.imageExtent      = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage =
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.preTransform     = capabilities.currentTransform;
        createInfo.compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        createInfo.presentMode      = chosenPresentMode;
        createInfo.clipped          = VK_TRUE;

        _swapchain                  = _device->createSwapchainKHRUnique(createInfo);
        _swapchainFormat            = chosenFormat.format;
        _swapchainExtent            = extent;
        _swapchainImages            = _device->getSwapchainImagesKHR(_swapchain.get());
        _imageInitialized.assign(_swapchainImages.size(), 0);

        const std::array<vk::Format, 3> depthCandidates = {
            vk::Format::eD32Sfloat,
            vk::Format::eD32SfloatS8Uint,
            vk::Format::eD24UnormS8Uint,
        };

        _depthFormat = vk::Format::eUndefined;
        for (auto candidate : depthCandidates)
        {
            auto props = _physicalDevice.getFormatProperties(candidate);
            if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
            {
                _depthFormat = candidate;
                break;
            }
        }

        if (_depthFormat == vk::Format::eUndefined)
        {
            std::cerr << "No supported depth format found" << std::endl;
            return false;
        }

        vk::ImageCreateInfo depthImageInfo;
        depthImageInfo.imageType     = vk::ImageType::e2D;
        depthImageInfo.extent.width  = _swapchainExtent.width;
        depthImageInfo.extent.height = _swapchainExtent.height;
        depthImageInfo.extent.depth  = 1;
        depthImageInfo.mipLevels     = 1;
        depthImageInfo.arrayLayers   = 1;
        depthImageInfo.format        = _depthFormat;
        depthImageInfo.tiling        = vk::ImageTiling::eOptimal;
        depthImageInfo.initialLayout = vk::ImageLayout::eUndefined;
        depthImageInfo.usage         = vk::ImageUsageFlagBits::eDepthStencilAttachment;
        depthImageInfo.samples       = vk::SampleCountFlagBits::e1;
        depthImageInfo.sharingMode   = vk::SharingMode::eExclusive;

        _depthImage                  = _device->createImageUnique(depthImageInfo);

        auto depthMemReq             = _device->getImageMemoryRequirements(_depthImage.get());
        vk::MemoryAllocateInfo depthAllocInfo;
        depthAllocInfo.allocationSize = depthMemReq.size;
        depthAllocInfo.memoryTypeIndex =
            findMemoryType(depthMemReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

        _depthImageMemory = _device->allocateMemoryUnique(depthAllocInfo);
        _device->bindImageMemory(_depthImage.get(), _depthImageMemory.get(), 0);

        vk::ImageViewCreateInfo depthViewInfo;
        depthViewInfo.image                           = _depthImage.get();
        depthViewInfo.viewType                        = vk::ImageViewType::e2D;
        depthViewInfo.format                          = _depthFormat;
        depthViewInfo.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eDepth;
        depthViewInfo.subresourceRange.baseMipLevel   = 0;
        depthViewInfo.subresourceRange.levelCount     = 1;
        depthViewInfo.subresourceRange.baseArrayLayer = 0;
        depthViewInfo.subresourceRange.layerCount     = 1;

        _depthImageView = _device->createImageViewUnique(depthViewInfo);

        _swapchainImageViews.clear();
        _swapchainImageViews.reserve(_swapchainImages.size());
        for (const auto &image : _swapchainImages)
        {
            vk::ImageViewCreateInfo imageViewInfo;
            imageViewInfo.image                           = image;
            imageViewInfo.viewType                        = vk::ImageViewType::e2D;
            imageViewInfo.format                          = _swapchainFormat;
            imageViewInfo.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
            imageViewInfo.subresourceRange.baseMipLevel   = 0;
            imageViewInfo.subresourceRange.levelCount     = 1;
            imageViewInfo.subresourceRange.baseArrayLayer = 0;
            imageViewInfo.subresourceRange.layerCount     = 1;
            _swapchainImageViews.push_back(_device->createImageViewUnique(imageViewInfo));
        }

        vk::AttachmentDescription colorAttachment;
        colorAttachment.format         = _swapchainFormat;
        colorAttachment.samples        = vk::SampleCountFlagBits::e1;
        colorAttachment.loadOp         = vk::AttachmentLoadOp::eClear;
        colorAttachment.storeOp        = vk::AttachmentStoreOp::eStore;
        colorAttachment.stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
        colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        colorAttachment.initialLayout  = vk::ImageLayout::eColorAttachmentOptimal;
        colorAttachment.finalLayout    = vk::ImageLayout::eColorAttachmentOptimal;

        vk::AttachmentReference colorAttachmentRef;
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout     = vk::ImageLayout::eColorAttachmentOptimal;

        vk::AttachmentDescription depthAttachment;
        depthAttachment.format         = _depthFormat;
        depthAttachment.samples        = vk::SampleCountFlagBits::e1;
        depthAttachment.loadOp         = vk::AttachmentLoadOp::eClear;
        depthAttachment.storeOp        = vk::AttachmentStoreOp::eDontCare;
        depthAttachment.stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
        depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        depthAttachment.initialLayout  = vk::ImageLayout::eUndefined;
        depthAttachment.finalLayout    = vk::ImageLayout::eDepthStencilAttachmentOptimal;

        vk::AttachmentReference depthAttachmentRef;
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout     = vk::ImageLayout::eDepthStencilAttachmentOptimal;

        vk::SubpassDescription subpass;
        subpass.pipelineBindPoint       = vk::PipelineBindPoint::eGraphics;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        vk::SubpassDependency dependency;
        dependency.srcSubpass   = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass   = 0;
        dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                  vk::PipelineStageFlagBits::eEarlyFragmentTests;
        dependency.srcAccessMask = {};
        dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                                   vk::AccessFlagBits::eDepthStencilAttachmentWrite;

        std::array<vk::AttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

        vk::RenderPassCreateInfo renderPassInfo;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments    = attachments.data();
        renderPassInfo.subpassCount    = 1;
        renderPassInfo.pSubpasses      = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies   = &dependency;
        _renderPass                    = _device->createRenderPassUnique(renderPassInfo);

        auto vertShaderCode =
            loadBinaryFile({"build/bin/shaders/cube.vert.spv", "bin/shaders/cube.vert.spv",
                            "shaders/cube.vert.spv"});
        auto fragShaderCode =
            loadBinaryFile({"build/bin/shaders/cube.frag.spv", "bin/shaders/cube.frag.spv",
                            "shaders/cube.frag.spv"});
        if (vertShaderCode.empty() || fragShaderCode.empty())
        {
            std::cerr << "Failed to load shader binaries" << std::endl;
            return false;
        }

        vk::ShaderModuleCreateInfo vertShaderInfo;
        vertShaderInfo.codeSize = vertShaderCode.size();
        vertShaderInfo.pCode    = reinterpret_cast<const uint32_t *>(vertShaderCode.data());
        auto vertShaderModule   = _device->createShaderModuleUnique(vertShaderInfo);

        vk::ShaderModuleCreateInfo fragShaderInfo;
        fragShaderInfo.codeSize = fragShaderCode.size();
        fragShaderInfo.pCode    = reinterpret_cast<const uint32_t *>(fragShaderCode.data());
        auto fragShaderModule   = _device->createShaderModuleUnique(fragShaderInfo);

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
        vertShaderStageInfo.stage  = vk::ShaderStageFlagBits::eVertex;
        vertShaderStageInfo.module = vertShaderModule.get();
        vertShaderStageInfo.pName  = "main";

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
        fragShaderStageInfo.stage  = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = fragShaderModule.get();
        fragShaderStageInfo.pName  = "main";

        std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {vertShaderStageInfo,
                                                                         fragShaderStageInfo};

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
        vertexInputInfo.vertexBindingDescriptionCount   = 0;
        vertexInputInfo.pVertexBindingDescriptions      = nullptr;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions    = nullptr;

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
        inputAssembly.topology               = vk::PrimitiveTopology::eTriangleList;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        vk::Viewport viewport;
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = static_cast<float>(_swapchainExtent.width);
        viewport.height   = static_cast<float>(_swapchainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vk::Rect2D scissor;
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent   = _swapchainExtent;

        vk::PipelineViewportStateCreateInfo viewportState;
        viewportState.viewportCount = 1;
        viewportState.pViewports    = &viewport;
        viewportState.scissorCount  = 1;
        viewportState.pScissors     = &scissor;

        vk::PipelineRasterizationStateCreateInfo rasterizer;
        rasterizer.depthClampEnable        = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode             = vk::PolygonMode::eFill;
        rasterizer.lineWidth               = 1.0f;
        rasterizer.cullMode                = vk::CullModeFlagBits::eNone;
        rasterizer.frontFace               = vk::FrontFace::eCounterClockwise;
        rasterizer.depthBiasEnable         = VK_FALSE;

        vk::PipelineMultisampleStateCreateInfo multisampling;
        multisampling.sampleShadingEnable  = VK_FALSE;
        multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

        vk::PipelineDepthStencilStateCreateInfo depthStencil;
        depthStencil.depthTestEnable       = VK_TRUE;
        depthStencil.depthWriteEnable      = VK_TRUE;
        depthStencil.depthCompareOp        = vk::CompareOp::eLess;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable     = VK_FALSE;

        vk::PipelineColorBlendAttachmentState colorBlendAttachment;
        colorBlendAttachment.colorWriteMask =
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        colorBlendAttachment.blendEnable = VK_FALSE;

        vk::PipelineColorBlendStateCreateInfo colorBlending;
        colorBlending.logicOpEnable   = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments    = &colorBlendAttachment;

        vk::PushConstantRange pushConstantRange;
        pushConstantRange.stageFlags =
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        pushConstantRange.offset = 0;
        pushConstantRange.size   = sizeof(VulkanAPI::PushConstants);

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
        pipelineLayoutInfo.setLayoutCount         = 0;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;
        _pipelineLayout = _device->createPipelineLayoutUnique(pipelineLayoutInfo);

        vk::GraphicsPipelineCreateInfo pipelineInfo;
        pipelineInfo.stageCount          = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages             = shaderStages.data();
        pipelineInfo.pVertexInputState   = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState      = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState   = &multisampling;
        pipelineInfo.pDepthStencilState  = &depthStencil;
        pipelineInfo.pColorBlendState    = &colorBlending;
        pipelineInfo.layout              = _pipelineLayout.get();
        pipelineInfo.renderPass          = _renderPass.get();
        pipelineInfo.subpass             = 0;

        auto pipelineResult =
            _device->createGraphicsPipelineUnique(vk::PipelineCache{}, pipelineInfo);
        if (pipelineResult.result != vk::Result::eSuccess)
        {
            std::cerr << "Failed to create graphics pipeline" << std::endl;
            return false;
        }
        _graphicsPipeline = std::move(pipelineResult.value);

        // --- Pyramid pipeline ---
        auto pyramidVertCode =
            loadBinaryFile({"build/bin/shaders/pyramid.vert.spv", "bin/shaders/pyramid.vert.spv",
                            "shaders/pyramid.vert.spv"});
        auto pyramidFragCode =
            loadBinaryFile({"build/bin/shaders/pyramid.frag.spv", "bin/shaders/pyramid.frag.spv",
                            "shaders/pyramid.frag.spv"});
        if (pyramidVertCode.empty() || pyramidFragCode.empty())
        {
            std::cerr << "Failed to load pyramid shader binaries" << std::endl;
            return false;
        }

        vk::ShaderModuleCreateInfo pyramidVertInfo;
        pyramidVertInfo.codeSize = pyramidVertCode.size();
        pyramidVertInfo.pCode    = reinterpret_cast<const uint32_t *>(pyramidVertCode.data());
        auto pyramidVertModule   = _device->createShaderModuleUnique(pyramidVertInfo);

        vk::ShaderModuleCreateInfo pyramidFragInfo;
        pyramidFragInfo.codeSize = pyramidFragCode.size();
        pyramidFragInfo.pCode    = reinterpret_cast<const uint32_t *>(pyramidFragCode.data());
        auto pyramidFragModule   = _device->createShaderModuleUnique(pyramidFragInfo);

        vk::PipelineShaderStageCreateInfo pyramidVertStage;
        pyramidVertStage.stage  = vk::ShaderStageFlagBits::eVertex;
        pyramidVertStage.module = pyramidVertModule.get();
        pyramidVertStage.pName  = "main";

        vk::PipelineShaderStageCreateInfo pyramidFragStage;
        pyramidFragStage.stage  = vk::ShaderStageFlagBits::eFragment;
        pyramidFragStage.module = pyramidFragModule.get();
        pyramidFragStage.pName  = "main";

        std::array<vk::PipelineShaderStageCreateInfo, 2> pyramidShaderStages = {pyramidVertStage,
                                                                                pyramidFragStage};
        pipelineInfo.pStages = pyramidShaderStages.data();

        auto pyramidPipelineResult =
            _device->createGraphicsPipelineUnique(vk::PipelineCache{}, pipelineInfo);
        if (pyramidPipelineResult.result != vk::Result::eSuccess)
        {
            std::cerr << "Failed to create pyramid graphics pipeline" << std::endl;
            return false;
        }
        _pyramidPipeline = std::move(pyramidPipelineResult.value);

        // --- Grid pipeline ---
        auto gridVertCode = loadBinaryFile({"build/bin/shaders/grid.vert.spv",
                                            "bin/shaders/grid.vert.spv", "shaders/grid.vert.spv"});
        auto gridFragCode = loadBinaryFile({"build/bin/shaders/grid.frag.spv",
                                            "bin/shaders/grid.frag.spv", "shaders/grid.frag.spv"});
        if (gridVertCode.empty() || gridFragCode.empty())
        {
            std::cerr << "Failed to load grid shader binaries" << std::endl;
            return false;
        }

        vk::ShaderModuleCreateInfo gridVertInfo;
        gridVertInfo.codeSize = gridVertCode.size();
        gridVertInfo.pCode    = reinterpret_cast<const uint32_t *>(gridVertCode.data());
        auto gridVertModule   = _device->createShaderModuleUnique(gridVertInfo);

        vk::ShaderModuleCreateInfo gridFragInfo;
        gridFragInfo.codeSize = gridFragCode.size();
        gridFragInfo.pCode    = reinterpret_cast<const uint32_t *>(gridFragCode.data());
        auto gridFragModule   = _device->createShaderModuleUnique(gridFragInfo);

        vk::PipelineShaderStageCreateInfo gridVertStage;
        gridVertStage.stage  = vk::ShaderStageFlagBits::eVertex;
        gridVertStage.module = gridVertModule.get();
        gridVertStage.pName  = "main";

        vk::PipelineShaderStageCreateInfo gridFragStage;
        gridFragStage.stage  = vk::ShaderStageFlagBits::eFragment;
        gridFragStage.module = gridFragModule.get();
        gridFragStage.pName  = "main";

        std::array<vk::PipelineShaderStageCreateInfo, 2> gridShaderStages = {gridVertStage,
                                                                             gridFragStage};
        pipelineInfo.pStages                                              = gridShaderStages.data();

        auto gridPipelineResult =
            _device->createGraphicsPipelineUnique(vk::PipelineCache{}, pipelineInfo);
        if (gridPipelineResult.result != vk::Result::eSuccess)
        {
            std::cerr << "Failed to create grid graphics pipeline" << std::endl;
            return false;
        }
        _gridPipeline = std::move(gridPipelineResult.value);

        // --- Sun pipeline ---
        auto sunVertCode = loadBinaryFile(
            {"build/bin/shaders/sun.vert.spv", "bin/shaders/sun.vert.spv", "shaders/sun.vert.spv"});
        auto sunFragCode = loadBinaryFile(
            {"build/bin/shaders/sun.frag.spv", "bin/shaders/sun.frag.spv", "shaders/sun.frag.spv"});
        if (sunVertCode.empty() || sunFragCode.empty())
        {
            std::cerr << "Failed to load sun shader binaries" << std::endl;
            return false;
        }

        vk::ShaderModuleCreateInfo sunVertInfo;
        sunVertInfo.codeSize = sunVertCode.size();
        sunVertInfo.pCode    = reinterpret_cast<const uint32_t *>(sunVertCode.data());
        auto sunVertModule   = _device->createShaderModuleUnique(sunVertInfo);

        vk::ShaderModuleCreateInfo sunFragInfo;
        sunFragInfo.codeSize = sunFragCode.size();
        sunFragInfo.pCode    = reinterpret_cast<const uint32_t *>(sunFragCode.data());
        auto sunFragModule   = _device->createShaderModuleUnique(sunFragInfo);

        vk::PipelineShaderStageCreateInfo sunVertStage;
        sunVertStage.stage  = vk::ShaderStageFlagBits::eVertex;
        sunVertStage.module = sunVertModule.get();
        sunVertStage.pName  = "main";

        vk::PipelineShaderStageCreateInfo sunFragStage;
        sunFragStage.stage  = vk::ShaderStageFlagBits::eFragment;
        sunFragStage.module = sunFragModule.get();
        sunFragStage.pName  = "main";

        std::array<vk::PipelineShaderStageCreateInfo, 2> sunShaderStages = {sunVertStage,
                                                                            sunFragStage};
        pipelineInfo.pStages                                             = sunShaderStages.data();

        auto sunPipelineResult =
            _device->createGraphicsPipelineUnique(vk::PipelineCache{}, pipelineInfo);
        if (sunPipelineResult.result != vk::Result::eSuccess)
        {
            std::cerr << "Failed to create sun graphics pipeline" << std::endl;
            return false;
        }
        _sunPipeline = std::move(sunPipelineResult.value);

        _swapchainFramebuffers.clear();
        _swapchainFramebuffers.reserve(_swapchainImageViews.size());
        for (const auto &imageView : _swapchainImageViews)
        {
            vk::FramebufferCreateInfo framebufferInfo;
            framebufferInfo.renderPass                          = _renderPass.get();
            std::array<vk::ImageView, 2> framebufferAttachments = {
                imageView.get(),
                _depthImageView.get(),
            };
            framebufferInfo.attachmentCount = static_cast<uint32_t>(framebufferAttachments.size());
            framebufferInfo.pAttachments    = framebufferAttachments.data();
            framebufferInfo.width           = _swapchainExtent.width;
            framebufferInfo.height          = _swapchainExtent.height;
            framebufferInfo.layers          = 1;
            _swapchainFramebuffers.push_back(_device->createFramebufferUnique(framebufferInfo));
        }

        vk::CommandPoolCreateInfo commandPoolInfo;
        commandPoolInfo.queueFamilyIndex = _graphicsQueueFamily;
        commandPoolInfo.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        _commandPool                     = _device->createCommandPoolUnique(commandPoolInfo);

        vk::CommandBufferAllocateInfo cmdAllocInfo;
        cmdAllocInfo.commandPool        = _commandPool.get();
        cmdAllocInfo.level              = vk::CommandBufferLevel::ePrimary;
        cmdAllocInfo.commandBufferCount = static_cast<uint32_t>(_swapchainImages.size());
        _commandBuffers                 = _device->allocateCommandBuffers(cmdAllocInfo);

        vk::SemaphoreCreateInfo semaphoreInfo;
        vk::FenceCreateInfo fenceInfo;
        fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

        _imageAvailableSemaphores.clear();
        _renderFinishedSemaphores.clear();
        _inFlightFences.clear();

        _imageAvailableSemaphores.reserve(kMaxFramesInFlight);
        _renderFinishedSemaphores.reserve(_swapchainImages.size());
        _inFlightFences.reserve(kMaxFramesInFlight);

        for (std::size_t i = 0; i < kMaxFramesInFlight; ++i)
        {
            _imageAvailableSemaphores.push_back(_device->createSemaphoreUnique(semaphoreInfo));
            _inFlightFences.push_back(_device->createFenceUnique(fenceInfo));
        }

        for (std::size_t i = 0; i < _swapchainImages.size(); ++i)
            _renderFinishedSemaphores.push_back(_device->createSemaphoreUnique(semaphoreInfo));

        _imagesInFlight.assign(_swapchainImages.size(), vk::Fence{});
        _currentFrame = 0;

        std::cout << "Vulkan swapchain created successfully: " << extent.width << "x"
                  << extent.height << std::endl;
        return true;
    } catch (const std::exception &e)
    {
        std::cerr << "Failed to create swapchain: " << e.what() << std::endl;
        return false;
    }
}

bool VulkanAPI::recreateSwapchain(uint32_t width, uint32_t height)
{
    if (!_surface)
        return false;

    return createSwapchain(width, height);
}

void VulkanAPI::setEntityModelMatrices(std::vector<glm::mat4> matrices)
{
    _entityModelMatrices = std::move(matrices);
}

bool VulkanAPI::drawFrame()
{
    if (!_device || !_swapchain || _inFlightFences.empty() || _imageAvailableSemaphores.empty() ||
        _renderFinishedSemaphores.empty())
        return false;

    try
    {
        const auto now     = std::chrono::steady_clock::now();
        float deltaSeconds = 0.0f;
        if (_lastFrameTime.time_since_epoch().count() != 0)
        {
            deltaSeconds = std::chrono::duration<float>(now - _lastFrameTime).count();
        }
        _lastFrameTime = now;

        // Clamp dt to avoid huge jumps after pauses, moving windows, or breakpoints.
        deltaSeconds                                   = std::clamp(deltaSeconds, 0.0f, 0.1f);

        constexpr float kCubeRotationSpeedRadPerSec    = 0.75f;
        constexpr float kPyramidRotationSpeedRadPerSec = 0.75f;
        _cubeRotationAngle    += kCubeRotationSpeedRadPerSec * deltaSeconds;
        _pyramidRotationAngle -= kPyramidRotationSpeedRadPerSec * deltaSeconds;

        glm::vec3 desiredDirection(0.0f);
        const glm::vec3 forwardPlanar = flatForward(_cameraYaw);
        const glm::vec3 rightPlanar =
            glm::normalize(glm::cross(forwardPlanar, glm::vec3(0.0f, 1.0f, 0.0f)));

        if (_moveForwardActive)
            desiredDirection += forwardPlanar;
        if (_moveBackwardActive)
            desiredDirection -= forwardPlanar;
        if (_moveRightActive)
            desiredDirection += rightPlanar;
        if (_moveLeftActive)
            desiredDirection -= rightPlanar;
        if (_moveUpActive)
            desiredDirection += glm::vec3(0.0f, 1.0f, 0.0f);
        if (_moveDownActive)
            desiredDirection -= glm::vec3(0.0f, 1.0f, 0.0f);

        glm::vec3 desiredVelocity(0.0f);
        if (glm::dot(desiredDirection, desiredDirection) > 0.0f)
        {
            const float sprintFactor = _sprintActive ? kCameraControls.sprintMultiplier : 1.0f;
            desiredVelocity          = glm::normalize(desiredDirection) *
                              (kCameraControls.moveSpeedUnitsPerSec * sprintFactor);
        }

        const float response  = 1.0f - std::exp(-kCameraControls.acceleration * deltaSeconds);
        _cameraVelocity      += (desiredVelocity - _cameraVelocity) * response;

        if (glm::dot(_cameraVelocity, _cameraVelocity) <
            kCameraControls.stopVelocityEpsilon * kCameraControls.stopVelocityEpsilon)
            _cameraVelocity = glm::vec3(0.0f);

        _cameraPosition += _cameraVelocity * deltaSeconds;

        if (_swapchainRecreateRequested)
        {
            if (!recreateSwapchainFromSurfaceExtent())
                return false;

            _swapchainRecreateRequested = false;
        }

        auto &inFlightFence        = _inFlightFences[_currentFrame];
        auto fence                 = inFlightFence.get();
        const auto fenceWaitResult = _device->waitForFences(1, &fence, VK_TRUE, 0);
        if (fenceWaitResult == vk::Result::eTimeout)
            return false;

        if (fenceWaitResult != vk::Result::eSuccess)
            return false;

        bool requestRecreate             = false;

        auto &imageAvailableSemaphore    = _imageAvailableSemaphores[_currentFrame];
        auto [acquireResult, imageIndex] = _device->acquireNextImageKHR(
            _swapchain.get(), 0, imageAvailableSemaphore.get(), vk::Fence{});

        if (acquireResult == vk::Result::eErrorOutOfDateKHR)
        {
            _swapchainRecreateRequested = true;
            return false;
        }

        if (acquireResult == vk::Result::eTimeout || acquireResult == vk::Result::eNotReady)
            return false;

        if (acquireResult != vk::Result::eSuccess && acquireResult != vk::Result::eSuboptimalKHR)
            return false;

        if (acquireResult == vk::Result::eSuboptimalKHR)
            requestRecreate = true;

        if (imageIndex >= _commandBuffers.size() || imageIndex >= _swapchainImages.size() ||
            imageIndex >= _swapchainFramebuffers.size())
            return false;

        if (imageIndex < _imagesInFlight.size() && _imagesInFlight[imageIndex])
        {
            auto imageFence = _imagesInFlight[imageIndex];
            const auto imageFenceWaitResult =
                _device->waitForFences(1, &imageFence, VK_TRUE, UINT64_MAX);

            if (imageFenceWaitResult != vk::Result::eSuccess)
                return false;
        }

        static_cast<void>(_device->resetFences(1, &fence));

        if (imageIndex < _imagesInFlight.size())
            _imagesInFlight[imageIndex] = fence;
        auto commandBuffer = _commandBuffers[imageIndex];
        commandBuffer.reset();

        vk::CommandBufferBeginInfo beginInfo;
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        commandBuffer.begin(beginInfo);

        const bool isInitialized =
            imageIndex < _imageInitialized.size() && _imageInitialized[imageIndex] != 0;

        vk::ImageMemoryBarrier toColorAttachment;
        toColorAttachment.oldLayout =
            isInitialized ? vk::ImageLayout::ePresentSrcKHR : vk::ImageLayout::eUndefined;
        toColorAttachment.newLayout                     = vk::ImageLayout::eColorAttachmentOptimal;
        toColorAttachment.srcQueueFamilyIndex           = VK_QUEUE_FAMILY_IGNORED;
        toColorAttachment.dstQueueFamilyIndex           = VK_QUEUE_FAMILY_IGNORED;
        toColorAttachment.image                         = _swapchainImages[imageIndex];
        toColorAttachment.subresourceRange.aspectMask   = vk::ImageAspectFlagBits::eColor;
        toColorAttachment.subresourceRange.baseMipLevel = 0;
        toColorAttachment.subresourceRange.levelCount   = 1;
        toColorAttachment.subresourceRange.baseArrayLayer = 0;
        toColorAttachment.subresourceRange.layerCount     = 1;
        toColorAttachment.srcAccessMask =
            isInitialized ? vk::AccessFlagBits::eMemoryRead : vk::AccessFlags{};
        toColorAttachment.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

        commandBuffer.pipelineBarrier(isInitialized ? vk::PipelineStageFlagBits::eBottomOfPipe
                                                    : vk::PipelineStageFlagBits::eTopOfPipe,
                                      vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, 0,
                                      nullptr, 0, nullptr, 1, &toColorAttachment);

        // Динамічне небо: колір залежить від висоти сонця
        const glm::vec3 sunlightDirection = glm::normalize(glm::vec3(-0.45f, -1.0f, -0.30f));
        const float sunElevation          = glm::clamp(-sunlightDirection.y, 0.0f, 1.0f);
        const glm::vec3 kSkyDay(0.36f, 0.56f, 0.90f);
        const glm::vec3 kSkyNight(0.01f, 0.01f, 0.06f);
        const glm::vec3 skyColor =
            glm::mix(kSkyNight, kSkyDay, glm::clamp(sunElevation * 1.5f, 0.0f, 1.0f));

        std::array<vk::ClearValue, 2> clearValues{};
        clearValues[0].color =
            vk::ClearColorValue(std::array<float, 4>{skyColor.x, skyColor.y, skyColor.z, 1.0f});
        clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

        vk::RenderPassBeginInfo renderPassInfo;
        renderPassInfo.renderPass          = _renderPass.get();
        renderPassInfo.framebuffer         = _swapchainFramebuffers[imageIndex].get();
        renderPassInfo.renderArea.offset.x = 0;
        renderPassInfo.renderArea.offset.y = 0;
        renderPassInfo.renderArea.extent   = _swapchainExtent;
        renderPassInfo.clearValueCount     = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues        = clearValues.data();

        commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

        const float aspect = static_cast<float>(_swapchainExtent.width) /
                             static_cast<float>((std::max)(_swapchainExtent.height, 1u));
        const glm::vec3 forward = cameraForward(_cameraYaw, _cameraPitch);
        const glm::mat4 view =
            glm::lookAt(_cameraPosition, _cameraPosition + forward, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 projection = glm::perspective(
            glm::radians(60.0f), aspect, kCameraControls.nearPlane, kCameraControls.farPlane);
        projection[1][1] *= -1.0f;

        VulkanAPI::PushConstants pushConstants{};
        pushConstants.viewProjection = projection * view;
        pushConstants.animationData =
            glm::vec4(_cubeRotationAngle, _pyramidRotationAngle, aspect, 0.0f);
        pushConstants.sunDirectionIntensity = glm::vec4(sunlightDirection, 4.5f);
        pushConstants.lightingParams        = glm::vec4(0.06f, // ambient
                                                        0.94f, // diffuse multiplier
                                                        0.0f,  // grid brightness boost
                                                        0.0f   // reserved
               );
        pushConstants.modelMatrix           = glm::mat4(1.0f);

        // Draw ECS entities with cube pipeline
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _graphicsPipeline.get());
        for (const auto &modelMatrix : _entityModelMatrices)
        {
            pushConstants.modelMatrix = modelMatrix;
            commandBuffer.pushConstants(_pipelineLayout.get(),
                                        vk::ShaderStageFlagBits::eVertex |
                                            vk::ShaderStageFlagBits::eFragment,
                                        0, sizeof(PushConstants), &pushConstants);
            commandBuffer.draw(36, 1, 0, 0);
        }

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _pyramidPipeline.get());
        pushConstants.modelMatrix = glm::mat4(1.0f);
        commandBuffer.pushConstants(_pipelineLayout.get(),
                                    vk::ShaderStageFlagBits::eVertex |
                                        vk::ShaderStageFlagBits::eFragment,
                                    0, sizeof(PushConstants), &pushConstants);
        commandBuffer.draw(18, 1, 0, 0);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _gridPipeline.get());
        commandBuffer.pushConstants(_pipelineLayout.get(),
                                    vk::ShaderStageFlagBits::eVertex |
                                        vk::ShaderStageFlagBits::eFragment,
                                    0, sizeof(PushConstants), &pushConstants);
        commandBuffer.draw(6, 1, 0, 0);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _sunPipeline.get());
        commandBuffer.pushConstants(_pipelineLayout.get(),
                                    vk::ShaderStageFlagBits::eVertex |
                                        vk::ShaderStageFlagBits::eFragment,
                                    0, sizeof(PushConstants), &pushConstants);
        commandBuffer.draw(768, 1, 0, 0); // UV-сфера: 16 lon x 8 lat x 2 tri x 3 vert

        commandBuffer.endRenderPass();

        vk::ImageMemoryBarrier toPresent;
        toPresent.oldLayout                       = vk::ImageLayout::eColorAttachmentOptimal;
        toPresent.newLayout                       = vk::ImageLayout::ePresentSrcKHR;
        toPresent.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        toPresent.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        toPresent.image                           = _swapchainImages[imageIndex];
        toPresent.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
        toPresent.subresourceRange.baseMipLevel   = 0;
        toPresent.subresourceRange.levelCount     = 1;
        toPresent.subresourceRange.baseArrayLayer = 0;
        toPresent.subresourceRange.layerCount     = 1;
        toPresent.srcAccessMask                   = vk::AccessFlagBits::eColorAttachmentWrite;
        toPresent.dstAccessMask                   = vk::AccessFlagBits::eMemoryRead;

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                      vk::PipelineStageFlagBits::eBottomOfPipe, {}, 0, nullptr, 0,
                                      nullptr, 1, &toPresent);

        commandBuffer.end();

        vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        auto waitSemaphore               = imageAvailableSemaphore.get();
        auto signalSemaphore             = _renderFinishedSemaphores[imageIndex].get();

        vk::SubmitInfo submitInfo;
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = &waitSemaphore;
        submitInfo.pWaitDstStageMask    = &waitStage;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &signalSemaphore;

        const auto submitResult = _graphicsQueue.submit(1, &submitInfo, inFlightFence.get());
        if (submitResult != vk::Result::eSuccess)
            return false;

        vk::PresentInfoKHR presentInfo;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &signalSemaphore;
        presentInfo.swapchainCount     = 1;
        auto swapchain                 = _swapchain.get();
        presentInfo.pSwapchains        = &swapchain;
        presentInfo.pImageIndices      = &imageIndex;

        auto presentResult             = _presentQueue.presentKHR(presentInfo);
        if (presentResult == vk::Result::eErrorOutOfDateKHR ||
            presentResult == vk::Result::eSuboptimalKHR)
            requestRecreate = true;

        if (imageIndex < _imageInitialized.size())
            _imageInitialized[imageIndex] = 1;

        _currentFrame = (_currentFrame + 1) % kMaxFramesInFlight;

        if (requestRecreate)
            _swapchainRecreateRequested = true;

        return presentResult == vk::Result::eSuccess;

    } catch (const std::exception &e)
    {
        const std::string message = e.what();
        if (message.find("ErrorOutOfDateKHR") != std::string::npos ||
            message.find("SuboptimalKHR") != std::string::npos)
        {
            _swapchainRecreateRequested = true;
            return false;
        }

        std::cerr << "drawFrame failed: " << e.what() << std::endl;
        return false;
    }
}

uint32_t VulkanAPI::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
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

#include "vulkandevice.h"
#include "vulkanswapchain.h"

#include <array>
#include <iostream>
#include <limits>


using namespace vigine::graphics;

VulkanSwapchain::VulkanSwapchain(VulkanDevice &device) : _device(device) {}

VulkanSwapchain::~VulkanSwapchain() { cleanup(); }

// ---------------------------------------------------------------------------
// Public lifecycle
// ---------------------------------------------------------------------------

bool VulkanSwapchain::setup(uint32_t width, uint32_t height, uint32_t pushConstantSize)
{
    _pushConstantSize = pushConstantSize;
    ++_swapchainGeneration;

    if (!createSwapchainImages(width, height))
        return false;
    if (!createDepthResources())
        return false;
    if (!createRenderPass())
        return false;
    if (!createFramebuffers())
        return false;
    if (!createCommandResources())
        return false;
    if (!createSyncPrimitives())
        return false;

    return true;
}

void VulkanSwapchain::cleanup()
{
    if (!_device.device())
        return;

    _device.device().waitIdle();

    _commandBuffers.clear();
    _commandPool.reset();

    _inFlightFences.clear();
    _renderFinishedSemaphores.clear();
    _imageAvailableSemaphores.clear();
    _imagesInFlight.clear();
    _currentFrame = 0;

    _swapchainFramebuffers.clear();
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

// ---------------------------------------------------------------------------
// Acquire / present
// ---------------------------------------------------------------------------

bool VulkanSwapchain::acquireImage(uint32_t &imageIndex, bool &requestRecreate)
{
    auto &inFlightFence        = _inFlightFences[_currentFrame];
    auto fence                 = inFlightFence.get();
    const auto fenceWaitResult = _device.device().waitForFences(1, &fence, VK_TRUE, 0);
    if (fenceWaitResult == vk::Result::eTimeout || fenceWaitResult != vk::Result::eSuccess)
        return false;

    auto &imageAvailableSemaphore  = _imageAvailableSemaphores[_currentFrame];
    auto [acquireResult, acqIndex] = _device.device().acquireNextImageKHR(
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

    imageIndex = acqIndex;

    if (imageIndex >= _commandBuffers.size() || imageIndex >= _swapchainImages.size() ||
        imageIndex >= _swapchainFramebuffers.size())
        return false;

    if (imageIndex < _imagesInFlight.size() && _imagesInFlight[imageIndex])
    {
        auto imageFence = _imagesInFlight[imageIndex];
        if (_device.device().waitForFences(1, &imageFence, VK_TRUE, UINT64_MAX) !=
            vk::Result::eSuccess)
            return false;
    }

    static_cast<void>(_device.device().resetFences(1, &fence));
    if (imageIndex < _imagesInFlight.size())
        _imagesInFlight[imageIndex] = fence;

    return true;
}

bool VulkanSwapchain::present(vk::CommandBuffer cmd, uint32_t imageIndex, bool &requestRecreate)
{
    auto &inFlightFence              = _inFlightFences[_currentFrame];
    auto &imageAvailableSem          = _imageAvailableSemaphores[_currentFrame];

    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    auto waitSemaphore               = imageAvailableSem.get();
    auto signalSemaphore             = _renderFinishedSemaphores[imageIndex].get();

    vk::SubmitInfo submitInfo;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &waitSemaphore;
    submitInfo.pWaitDstStageMask    = &waitStage;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &signalSemaphore;

    if (_device.graphicsQueue().submit(1, &submitInfo, inFlightFence.get()) != vk::Result::eSuccess)
        return false;

    vk::PresentInfoKHR presentInfo;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &signalSemaphore;
    presentInfo.swapchainCount     = 1;
    auto swapchain                 = _swapchain.get();
    presentInfo.pSwapchains        = &swapchain;
    presentInfo.pImageIndices      = &imageIndex;

    auto presentResult             = _device.presentQueue().presentKHR(presentInfo);
    if (presentResult == vk::Result::eErrorOutOfDateKHR ||
        presentResult == vk::Result::eSuboptimalKHR)
        requestRecreate = true;

    return presentResult == vk::Result::eSuccess || presentResult == vk::Result::eSuboptimalKHR;
}

// ---------------------------------------------------------------------------
// Private creation helpers
// ---------------------------------------------------------------------------

bool VulkanSwapchain::createSwapchainImages(uint32_t width, uint32_t height)
{
    auto capabilities = _device.physicalDevice().getSurfaceCapabilitiesKHR(_device.surface());
    auto formats      = _device.physicalDevice().getSurfaceFormatsKHR(_device.surface());
    auto presentModes = _device.physicalDevice().getSurfacePresentModesKHR(_device.surface());

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
        extent.width =
            std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(height, capabilities.minImageExtent.height,
                                   capabilities.maxImageExtent.height);
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
        imageCount = capabilities.maxImageCount;

    vk::SwapchainCreateInfoKHR createInfo;
    createInfo.surface          = _device.surface();
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

    _swapchain                  = _device.device().createSwapchainKHRUnique(createInfo);
    _swapchainFormat            = chosenFormat.format;
    _swapchainExtent            = extent;
    _swapchainImages            = _device.device().getSwapchainImagesKHR(_swapchain.get());
    _imageInitialized.assign(_swapchainImages.size(), 0);

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
        _swapchainImageViews.push_back(_device.device().createImageViewUnique(imageViewInfo));
    }

    return true;
}

bool VulkanSwapchain::createDepthResources()
{
    const std::array<vk::Format, 3> depthCandidates = {
        vk::Format::eD32Sfloat,
        vk::Format::eD32SfloatS8Uint,
        vk::Format::eD24UnormS8Uint,
    };

    _depthFormat = vk::Format::eUndefined;
    for (auto candidate : depthCandidates)
    {
        auto props = _device.physicalDevice().getFormatProperties(candidate);
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

    _depthImage                  = _device.device().createImageUnique(depthImageInfo);

    auto depthMemReq             = _device.device().getImageMemoryRequirements(_depthImage.get());
    vk::MemoryAllocateInfo depthAllocInfo;
    depthAllocInfo.allocationSize  = depthMemReq.size;
    depthAllocInfo.memoryTypeIndex = _device.findMemoryType(
        depthMemReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

    _depthImageMemory = _device.device().allocateMemoryUnique(depthAllocInfo);
    _device.device().bindImageMemory(_depthImage.get(), _depthImageMemory.get(), 0);

    vk::ImageViewCreateInfo depthViewInfo;
    depthViewInfo.image                           = _depthImage.get();
    depthViewInfo.viewType                        = vk::ImageViewType::e2D;
    depthViewInfo.format                          = _depthFormat;
    depthViewInfo.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eDepth;
    depthViewInfo.subresourceRange.baseMipLevel   = 0;
    depthViewInfo.subresourceRange.levelCount     = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount     = 1;

    _depthImageView = _device.device().createImageViewUnique(depthViewInfo);
    return true;
}

bool VulkanSwapchain::createRenderPass()
{
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
    _renderPass                    = _device.device().createRenderPassUnique(renderPassInfo);

    // Default pipeline layout: push constants only (no descriptor sets).
    vk::PushConstantRange pushConstantRange;
    pushConstantRange.stageFlags =
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    pushConstantRange.offset = 0;
    pushConstantRange.size   = _pushConstantSize;

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.setLayoutCount         = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;
    _pipelineLayout = _device.device().createPipelineLayoutUnique(pipelineLayoutInfo);

    return true;
}

bool VulkanSwapchain::createFramebuffers()
{
    _swapchainFramebuffers.clear();
    _swapchainFramebuffers.reserve(_swapchainImageViews.size());
    for (const auto &imageView : _swapchainImageViews)
    {
        std::array<vk::ImageView, 2> framebufferAttachments = {
            imageView.get(),
            _depthImageView.get(),
        };
        vk::FramebufferCreateInfo framebufferInfo;
        framebufferInfo.renderPass      = _renderPass.get();
        framebufferInfo.attachmentCount = static_cast<uint32_t>(framebufferAttachments.size());
        framebufferInfo.pAttachments    = framebufferAttachments.data();
        framebufferInfo.width           = _swapchainExtent.width;
        framebufferInfo.height          = _swapchainExtent.height;
        framebufferInfo.layers          = 1;
        _swapchainFramebuffers.push_back(_device.device().createFramebufferUnique(framebufferInfo));
    }
    return true;
}

bool VulkanSwapchain::createCommandResources()
{
    vk::CommandPoolCreateInfo commandPoolInfo;
    commandPoolInfo.queueFamilyIndex = _device.graphicsQueueFamily();
    commandPoolInfo.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    _commandPool                     = _device.device().createCommandPoolUnique(commandPoolInfo);

    vk::CommandBufferAllocateInfo cmdAllocInfo;
    cmdAllocInfo.commandPool        = _commandPool.get();
    cmdAllocInfo.level              = vk::CommandBufferLevel::ePrimary;
    cmdAllocInfo.commandBufferCount = static_cast<uint32_t>(_swapchainImages.size());
    _commandBuffers                 = _device.device().allocateCommandBuffers(cmdAllocInfo);
    return true;
}

bool VulkanSwapchain::createSyncPrimitives()
{
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
        _imageAvailableSemaphores.push_back(_device.device().createSemaphoreUnique(semaphoreInfo));
        _inFlightFences.push_back(_device.device().createFenceUnique(fenceInfo));
    }

    for (std::size_t i = 0; i < _swapchainImages.size(); ++i)
        _renderFinishedSemaphores.push_back(_device.device().createSemaphoreUnique(semaphoreInfo));

    _imagesInFlight.assign(_swapchainImages.size(), vk::Fence{});
    _currentFrame = 0;
    return true;
}

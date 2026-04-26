#include "vulkandevice.h"
#include "vulkantexturestore.h"

#include <algorithm>
#include <cstring>
#include <iostream>


using namespace vigine::ecs::graphics;

namespace
{
vk::DeviceSize computeTextureDataSize(TextureFormat format, uint32_t w, uint32_t h)
{
    switch (format)
    {
    case TextureFormat::R8_UNORM:
        return static_cast<vk::DeviceSize>(w) * h;
    case TextureFormat::RGBA8_UNORM:
    case TextureFormat::RGBA8_SRGB:
    case TextureFormat::BGRA8_SRGB:
    case TextureFormat::D32_SFLOAT:
    default:
        return static_cast<vk::DeviceSize>(w) * h * 4;
    }
}

vk::Format toVkTextureFormat(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat::R8_UNORM:
        return vk::Format::eR8Unorm;
    case TextureFormat::RGBA8_UNORM:
        return vk::Format::eR8G8B8A8Unorm;
    case TextureFormat::RGBA8_SRGB:
        return vk::Format::eR8G8B8A8Srgb;
    case TextureFormat::BGRA8_SRGB:
        return vk::Format::eB8G8R8A8Srgb;
    case TextureFormat::D32_SFLOAT:
        return vk::Format::eD32Sfloat;
    default:
        return vk::Format::eR8G8B8A8Unorm;
    }
}

vk::SamplerAddressMode toVkSamplerAddressMode(TextureWrapMode mode)
{
    switch (mode)
    {
    case TextureWrapMode::Repeat:
        return vk::SamplerAddressMode::eRepeat;
    case TextureWrapMode::ClampToEdge:
        return vk::SamplerAddressMode::eClampToEdge;
    case TextureWrapMode::ClampToBorder:
        return vk::SamplerAddressMode::eClampToBorder;
    default:
        return vk::SamplerAddressMode::eRepeat;
    }
}
} // namespace

VulkanTextureStore::VulkanTextureStore(VulkanDevice &device) : _device(device) {}

VulkanTextureStore::~VulkanTextureStore()
{
    // Wait for GPU to finish all work before destroying any resources.
    if (_device.device())
        _device.device().waitIdle();

    // Wait for all pending texture uploads to complete before freeing resources.
    if (_device.device() && !_pendingTextureUploads.empty())
    {
        std::vector<vk::Fence> fences;
        fences.reserve(_pendingTextureUploads.size());
        for (auto &staging : _pendingTextureUploads)
            fences.push_back(staging.fence.get());
        static_cast<void>(_device.device().waitForFences(fences, VK_TRUE, UINT64_MAX));
        _pendingTextureUploads.clear();
    }

    if (!_device.device())
        return;

    // Entity texture descriptor resources.
    _textureDescriptorSets.clear();
    _entityTextureDescriptorPool.reset();
    _entityTextureDescriptorSetLayout.reset();
    _entityTexturePipelineLayout.reset();

    // Remaining GPU textures (safety net — callers should destroy explicitly when possible).
    for (auto &[id, samp] : _textureSamplerHandles)
        _device.device().destroySampler(samp);
    _textureSamplerHandles.clear();

    for (auto &[id, view] : _textureViewHandles)
        _device.device().destroyImageView(view);
    _textureViewHandles.clear();

    for (auto &[id, img] : _textureHandles)
        _device.device().destroyImage(img);
    _textureHandles.clear();

    for (auto &[id, mem] : _textureMemoryHandles)
        _device.device().freeMemory(mem);
    _textureMemoryHandles.clear();

    _textureDescs.clear();
}

void VulkanTextureStore::initEntityTextureDescriptorResources(uint32_t pushConstantSize)
{
    // Device-scoped: only create once, survives swapchain recreation.
    if (_entityTextureDescriptorSetLayout)
        return;

    vk::DescriptorSetLayoutBinding texBinding;
    texBinding.binding         = 0;
    texBinding.descriptorType  = vk::DescriptorType::eCombinedImageSampler;
    texBinding.descriptorCount = 1;
    texBinding.stageFlags      = vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutCreateInfo texDslInfo;
    texDslInfo.bindingCount = 1;
    texDslInfo.pBindings    = &texBinding;
    _entityTextureDescriptorSetLayout =
        _device.device().createDescriptorSetLayoutUnique(texDslInfo);

    vk::DescriptorPoolSize texPoolSize;
    texPoolSize.type            = vk::DescriptorType::eCombinedImageSampler;
    texPoolSize.descriptorCount = 32;
    vk::DescriptorPoolCreateInfo texPoolInfo;
    texPoolInfo.maxSets          = 32;
    texPoolInfo.poolSizeCount    = 1;
    texPoolInfo.pPoolSizes       = &texPoolSize;
    _entityTextureDescriptorPool = _device.device().createDescriptorPoolUnique(texPoolInfo);

    vk::PushConstantRange pushConstantRange;
    pushConstantRange.stageFlags =
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    pushConstantRange.offset = 0;
    pushConstantRange.size   = pushConstantSize;

    auto texDsl              = _entityTextureDescriptorSetLayout.get();
    vk::PipelineLayoutCreateInfo texLayoutInfo;
    texLayoutInfo.setLayoutCount         = 1;
    texLayoutInfo.pSetLayouts            = &texDsl;
    texLayoutInfo.pushConstantRangeCount = 1;
    texLayoutInfo.pPushConstantRanges    = &pushConstantRange;
    _entityTexturePipelineLayout = _device.device().createPipelineLayoutUnique(texLayoutInfo);
}

TextureHandle VulkanTextureStore::createTexture(const TextureDesc &desc)
{
    if (!_device.device())
        return {};

    vk::Format vkFormat = toVkTextureFormat(desc.format);

    // Create image.
    vk::ImageCreateInfo imgInfo;
    imgInfo.imageType     = vk::ImageType::e2D;
    imgInfo.format        = vkFormat;
    imgInfo.extent        = vk::Extent3D{desc.width, desc.height, 1};
    imgInfo.mipLevels     = 1;
    imgInfo.arrayLayers   = 1;
    imgInfo.samples       = vk::SampleCountFlagBits::e1;
    imgInfo.tiling        = vk::ImageTiling::eOptimal;
    imgInfo.usage         = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
    imgInfo.sharingMode   = vk::SharingMode::eExclusive;
    imgInfo.initialLayout = vk::ImageLayout::eUndefined;

    vk::Image image       = _device.device().createImage(imgInfo);

    // Allocate device-local memory.
    auto memReq = _device.device().getImageMemoryRequirements(image);
    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex =
        _device.findMemoryType(memReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

    vk::DeviceMemory memory = _device.device().allocateMemory(allocInfo);
    _device.device().bindImageMemory(image, memory, 0);

    // Create image view.
    vk::ImageViewCreateInfo viewInfo;
    viewInfo.image                           = image;
    viewInfo.viewType                        = vk::ImageViewType::e2D;
    viewInfo.format                          = vkFormat;
    viewInfo.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    vk::ImageView view                       = _device.device().createImageView(viewInfo);

    // Create sampler.
    vk::SamplerCreateInfo sampInfo;
    sampInfo.magFilter =
        (desc.magFilter == TextureFilter::Linear) ? vk::Filter::eLinear : vk::Filter::eNearest;
    sampInfo.minFilter =
        (desc.minFilter == TextureFilter::Linear) ? vk::Filter::eLinear : vk::Filter::eNearest;
    sampInfo.addressModeU            = toVkSamplerAddressMode(desc.wrapU);
    sampInfo.addressModeV            = toVkSamplerAddressMode(desc.wrapV);
    sampInfo.addressModeW            = vk::SamplerAddressMode::eClampToEdge;
    sampInfo.anisotropyEnable        = VK_FALSE;
    sampInfo.maxAnisotropy           = 1.0f;
    sampInfo.borderColor             = vk::BorderColor::eFloatOpaqueBlack;
    sampInfo.unnormalizedCoordinates = VK_FALSE;
    sampInfo.compareEnable           = VK_FALSE;
    sampInfo.mipmapMode              = vk::SamplerMipmapMode::eLinear;

    vk::Sampler samp                 = _device.device().createSampler(sampInfo);

    // Store handles and metadata.
    TextureHandle handle;
    handle.value                         = _nextTextureHandleId++;
    _textureHandles[handle.value]        = image;
    _textureMemoryHandles[handle.value]  = memory;
    _textureViewHandles[handle.value]    = view;
    _textureSamplerHandles[handle.value] = samp;
    _textureDescs[handle.value]          = desc;

    return handle;
}

void VulkanTextureStore::uploadTexture(TextureHandle handle, const void *pixels, uint32_t width,
                                       uint32_t height)
{
    if (!_device.device() || !pixels)
        return;

    auto imgIt  = _textureHandles.find(handle.value);
    auto descIt = _textureDescs.find(handle.value);
    if (imgIt == _textureHandles.end() || descIt == _textureDescs.end())
        return;

    vk::Image image         = imgIt->second;
    const TextureDesc &desc = descIt->second;

    vk::DeviceSize dataSize = computeTextureDataSize(desc.format, width, height);

    // Clean up completed uploads before creating new ones.
    cleanupCompletedTextureUploads();

    // Create staging resources.
    TextureUploadStaging staging;

    vk::BufferCreateInfo stagingBufInfo;
    stagingBufInfo.size        = dataSize;
    stagingBufInfo.usage       = vk::BufferUsageFlagBits::eTransferSrc;
    stagingBufInfo.sharingMode = vk::SharingMode::eExclusive;
    staging.buffer             = _device.device().createBufferUnique(stagingBufInfo);

    auto stageReq              = _device.device().getBufferMemoryRequirements(staging.buffer.get());
    vk::MemoryAllocateInfo stageAlloc;
    stageAlloc.allocationSize  = stageReq.size;
    stageAlloc.memoryTypeIndex = _device.findMemoryType(
        stageReq.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    staging.memory = _device.device().allocateMemoryUnique(stageAlloc);
    _device.device().bindBufferMemory(staging.buffer.get(), staging.memory.get(), 0);

    // Copy pixel data to staging buffer.
    void *mapped = _device.device().mapMemory(staging.memory.get(), 0, dataSize);
    std::memcpy(mapped, pixels, static_cast<std::size_t>(dataSize));
    _device.device().unmapMemory(staging.memory.get());

    // Create one-time command pool and buffer.
    vk::CommandPoolCreateInfo tmpPoolInfo;
    tmpPoolInfo.queueFamilyIndex = _device.graphicsQueueFamily();
    tmpPoolInfo.flags            = vk::CommandPoolCreateFlagBits::eTransient;
    staging.commandPool          = _device.device().createCommandPoolUnique(tmpPoolInfo);

    vk::CommandBufferAllocateInfo cmdAlloc;
    cmdAlloc.commandPool        = staging.commandPool.get();
    cmdAlloc.level              = vk::CommandBufferLevel::ePrimary;
    cmdAlloc.commandBufferCount = 1;
    auto cmds                   = _device.device().allocateCommandBuffers(cmdAlloc);
    auto cmd                    = cmds[0];

    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmd.begin(beginInfo);

    recordImageLayoutTransitions(cmd, staging.buffer.get(), image, width, height);

    cmd.end();

    // Create fence for tracking completion (non-blocking submit).
    vk::FenceCreateInfo fenceInfo;
    staging.fence = _device.device().createFenceUnique(fenceInfo);

    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;
    static_cast<void>(_device.graphicsQueue().submit(1, &submitInfo, staging.fence.get()));

    _pendingTextureUploads.push_back(std::move(staging));
}

void VulkanTextureStore::destroyTexture(TextureHandle handle)
{
    auto imgIt     = _textureHandles.find(handle.value);
    auto memIt     = _textureMemoryHandles.find(handle.value);
    auto viewIt    = _textureViewHandles.find(handle.value);
    auto samplerIt = _textureSamplerHandles.find(handle.value);
    auto descIt    = _textureDescs.find(handle.value);

    if (samplerIt != _textureSamplerHandles.end())
    {
        _device.device().destroySampler(samplerIt->second);
        _textureSamplerHandles.erase(samplerIt);
    }

    if (viewIt != _textureViewHandles.end())
    {
        _device.device().destroyImageView(viewIt->second);
        _textureViewHandles.erase(viewIt);
    }

    if (imgIt != _textureHandles.end())
    {
        _device.device().destroyImage(imgIt->second);
        _textureHandles.erase(imgIt);
    }

    if (memIt != _textureMemoryHandles.end())
    {
        _device.device().freeMemory(memIt->second);
        _textureMemoryHandles.erase(memIt);
    }

    if (descIt != _textureDescs.end())
        _textureDescs.erase(descIt);
}

void VulkanTextureStore::createTextureDescriptorSet(TextureHandle handle)
{
    if (!handle.isValid() || !_entityTextureDescriptorSetLayout || !_entityTextureDescriptorPool)
        return;

    // Idempotent: skip if already created for this texture.
    if (_textureDescriptorSets.count(handle.value))
        return;

    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool     = _entityTextureDescriptorPool.get();
    allocInfo.descriptorSetCount = 1;
    auto dsl                     = _entityTextureDescriptorSetLayout.get();
    allocInfo.pSetLayouts        = &dsl;
    auto sets                    = _device.device().allocateDescriptorSets(allocInfo);

    vk::DescriptorImageInfo imgInfo;
    imgInfo.sampler     = sampler(handle);
    imgInfo.imageView   = imageView(handle);
    imgInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    vk::WriteDescriptorSet write;
    write.dstSet          = sets[0];
    write.dstBinding      = 0;
    write.descriptorCount = 1;
    write.descriptorType  = vk::DescriptorType::eCombinedImageSampler;
    write.pImageInfo      = &imgInfo;
    _device.device().updateDescriptorSets(1, &write, 0, nullptr);

    _textureDescriptorSets[handle.value] = sets[0];
}

void VulkanTextureStore::cleanupCompletedTextureUploads()
{
    if (!_device.device() || _pendingTextureUploads.empty())
        return;

    _pendingTextureUploads.erase(
        std::remove_if(_pendingTextureUploads.begin(), _pendingTextureUploads.end(),
                       [this](TextureUploadStaging &staging) {
                           auto result = _device.device().getFenceStatus(staging.fence.get());
                           return result == vk::Result::eSuccess;
                       }),
        _pendingTextureUploads.end());
}

vk::ImageView VulkanTextureStore::imageView(TextureHandle handle) const
{
    auto it = _textureViewHandles.find(handle.value);
    return (it != _textureViewHandles.end()) ? it->second : vk::ImageView{};
}

vk::Sampler VulkanTextureStore::sampler(TextureHandle handle) const
{
    auto it = _textureSamplerHandles.find(handle.value);
    return (it != _textureSamplerHandles.end()) ? it->second : vk::Sampler{};
}

vk::DescriptorSet VulkanTextureStore::entityTextureDescriptorSet(TextureHandle handle) const
{
    auto it = _textureDescriptorSets.find(handle.value);
    return (it != _textureDescriptorSets.end()) ? it->second : vk::DescriptorSet{};
}

void VulkanTextureStore::recordImageLayoutTransitions(vk::CommandBuffer cmd,
                                                      vk::Buffer stagingBuffer, vk::Image image,
                                                      uint32_t width, uint32_t height)
{
    // Transition to transfer destination.
    vk::ImageMemoryBarrier toTransfer;
    toTransfer.oldLayout                   = vk::ImageLayout::eUndefined;
    toTransfer.newLayout                   = vk::ImageLayout::eTransferDstOptimal;
    toTransfer.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image                       = image;
    toTransfer.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.layerCount = 1;
    toTransfer.srcAccessMask               = {};
    toTransfer.dstAccessMask               = vk::AccessFlagBits::eTransferWrite;
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer,
                        {}, 0, nullptr, 0, nullptr, 1, &toTransfer);

    // Copy buffer to image.
    vk::BufferImageCopy copyRegion;
    copyRegion.bufferOffset                    = 0;
    copyRegion.bufferRowLength                 = 0;
    copyRegion.bufferImageHeight               = 0;
    copyRegion.imageSubresource.aspectMask     = vk::ImageAspectFlagBits::eColor;
    copyRegion.imageSubresource.mipLevel       = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount     = 1;
    copyRegion.imageOffset                     = vk::Offset3D{0, 0, 0};
    copyRegion.imageExtent                     = vk::Extent3D{width, height, 1};
    cmd.copyBufferToImage(stagingBuffer, image, vk::ImageLayout::eTransferDstOptimal, 1,
                          &copyRegion);

    // Transition to shader read.
    vk::ImageMemoryBarrier toShaderRead;
    toShaderRead.oldLayout                   = vk::ImageLayout::eTransferDstOptimal;
    toShaderRead.newLayout                   = vk::ImageLayout::eShaderReadOnlyOptimal;
    toShaderRead.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
    toShaderRead.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
    toShaderRead.image                       = image;
    toShaderRead.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    toShaderRead.subresourceRange.levelCount = 1;
    toShaderRead.subresourceRange.layerCount = 1;
    toShaderRead.srcAccessMask               = vk::AccessFlagBits::eTransferWrite;
    toShaderRead.dstAccessMask               = vk::AccessFlagBits::eShaderRead;
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eFragmentShader, {}, 0, nullptr, 0, nullptr, 1,
                        &toShaderRead);
}

#include "vulkandevice.h"
#include "vulkanframerenderer.h"
#include "vulkanpipelinestore.h"
#include "vulkanswapchain.h"
#include "vulkantexturestore.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <iostream>
#include <limits>
#include <vector>


using namespace vigine::graphics;

namespace
{
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

// ---------------------------------------------------------------------------
// VulkanPerImageBuffer helpers
// ---------------------------------------------------------------------------

bool VulkanFrameRenderer::VulkanPerImageBuffer::ensureCapacity(vk::Device device,
                                                               uint32_t memTypeIndex,
                                                               vk::DeviceSize needed)
{
    if (needed <= capacity)
        return true;

    buffer.reset();
    memory.reset();

    vk::BufferCreateInfo bufInfo;
    bufInfo.size        = needed;
    bufInfo.usage       = vk::BufferUsageFlagBits::eVertexBuffer;
    bufInfo.sharingMode = vk::SharingMode::eExclusive;
    buffer              = device.createBufferUnique(bufInfo);

    auto memReq         = device.getBufferMemoryRequirements(buffer.get());
    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = memTypeIndex;
    memory                    = device.allocateMemoryUnique(allocInfo);
    device.bindBufferMemory(buffer.get(), memory.get(), 0);
    capacity = needed;
    return true;
}

void VulkanFrameRenderer::VulkanPerImageBuffer::upload(vk::Device device, const void *src,
                                                       vk::DeviceSize size)
{
    void *dst = device.mapMemory(memory.get(), 0, size);
    std::memcpy(dst, src, static_cast<std::size_t>(size));
    device.unmapMemory(memory.get());
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

VulkanFrameRenderer::VulkanFrameRenderer(VulkanDevice &device, VulkanSwapchain &swapchain,
                                         VulkanTextureStore &textureStore,
                                         VulkanPipelineStore &pipelineStore)
    : _device(device), _swapchain(swapchain), _textureStore(textureStore),
      _pipelineStore(pipelineStore)
{
}

// ---------------------------------------------------------------------------
// Swapchain lifecycle
// ---------------------------------------------------------------------------

void VulkanFrameRenderer::cleanupSwapchainResources()
{
    _instancedBufferStates.clear();
    _sdfGlyphBuffers.clear();
    _sdfGlyphUploadNeeded.clear();

    _sdfGlyphPipeline.reset();
    _sdfPipelineLayout.reset();
}

// ---------------------------------------------------------------------------
// SDF pipeline creation
// ---------------------------------------------------------------------------

void VulkanFrameRenderer::createSdfDescriptorResources()
{
    // Descriptor set layout: set 0, binding 0 = combined image sampler.
    vk::DescriptorSetLayoutBinding sdfBinding;
    sdfBinding.binding         = 0;
    sdfBinding.descriptorType  = vk::DescriptorType::eCombinedImageSampler;
    sdfBinding.descriptorCount = 1;
    sdfBinding.stageFlags      = vk::ShaderStageFlagBits::eFragment;
    vk::DescriptorSetLayoutCreateInfo sdfDslInfo;
    sdfDslInfo.bindingCount = 1;
    sdfDslInfo.pBindings    = &sdfBinding;
    _sdfDescriptorSetLayout = _device.device().createDescriptorSetLayoutUnique(sdfDslInfo);

    // Descriptor pool.
    vk::DescriptorPoolSize poolSize;
    poolSize.type            = vk::DescriptorType::eCombinedImageSampler;
    poolSize.descriptorCount = 1;
    vk::DescriptorPoolCreateInfo poolInfo;
    poolInfo.maxSets       = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    _sdfDescriptorPool     = _device.device().createDescriptorPoolUnique(poolInfo);

    // Allocate descriptor set.
    vk::DescriptorSetAllocateInfo sdfAllocInfo;
    sdfAllocInfo.descriptorPool     = _sdfDescriptorPool.get();
    sdfAllocInfo.descriptorSetCount = 1;
    auto sdfDsl                     = _sdfDescriptorSetLayout.get();
    sdfAllocInfo.pSetLayouts        = &sdfDsl;
    auto sdfSets                    = _device.device().allocateDescriptorSets(sdfAllocInfo);
    _sdfDescriptorSet               = sdfSets[0];

    // If atlas already uploaded — update descriptor set immediately.
    if (_sdfAtlasHandle.isValid())
    {
        vk::DescriptorImageInfo imgInfo;
        imgInfo.sampler     = _textureStore.sampler(_sdfAtlasHandle);
        imgInfo.imageView   = _textureStore.imageView(_sdfAtlasHandle);
        imgInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        vk::WriteDescriptorSet write;
        write.dstSet          = _sdfDescriptorSet;
        write.dstBinding      = 0;
        write.descriptorCount = 1;
        write.descriptorType  = vk::DescriptorType::eCombinedImageSampler;
        write.pImageInfo      = &imgInfo;
        _device.device().updateDescriptorSets(1, &write, 0, nullptr);
    }
}

void VulkanFrameRenderer::createSdfGraphicsPipelineObject(const std::vector<char> &vertCode,
                                                          const std::vector<char> &fragCode)
{
    vk::ShaderModuleCreateInfo sdfVertModInfo;
    sdfVertModInfo.codeSize = vertCode.size();
    sdfVertModInfo.pCode    = reinterpret_cast<const uint32_t *>(vertCode.data());
    auto sdfVertMod         = _device.device().createShaderModuleUnique(sdfVertModInfo);

    vk::ShaderModuleCreateInfo sdfFragModInfo;
    sdfFragModInfo.codeSize = fragCode.size();
    sdfFragModInfo.pCode    = reinterpret_cast<const uint32_t *>(fragCode.data());
    auto sdfFragMod         = _device.device().createShaderModuleUnique(sdfFragModInfo);

    vk::PipelineShaderStageCreateInfo sdfVertStage;
    sdfVertStage.stage  = vk::ShaderStageFlagBits::eVertex;
    sdfVertStage.module = sdfVertMod.get();
    sdfVertStage.pName  = "main";

    vk::PipelineShaderStageCreateInfo sdfFragStage;
    sdfFragStage.stage                                         = vk::ShaderStageFlagBits::eFragment;
    sdfFragStage.module                                        = sdfFragMod.get();
    sdfFragStage.pName                                         = "main";

    std::array<vk::PipelineShaderStageCreateInfo, 2> sdfStages = {sdfVertStage, sdfFragStage};

    // Vertex input: binding 0, per-vertex, stride = sizeof(GlyphQuadVertex).
    vk::VertexInputBindingDescription sdfVtxBinding;
    sdfVtxBinding.binding   = 0;
    sdfVtxBinding.stride    = sizeof(GlyphQuadVertex);
    sdfVtxBinding.inputRate = vk::VertexInputRate::eVertex;

    // Attribs: location 0 = vec3 pos, location 1 = vec2 uv.
    std::array<vk::VertexInputAttributeDescription, 2> sdfAttribs;
    sdfAttribs[0].binding  = 0;
    sdfAttribs[0].location = 0;
    sdfAttribs[0].format   = vk::Format::eR32G32B32Sfloat;
    sdfAttribs[0].offset   = offsetof(GlyphQuadVertex, pos);
    sdfAttribs[1].binding  = 0;
    sdfAttribs[1].location = 1;
    sdfAttribs[1].format   = vk::Format::eR32G32Sfloat;
    sdfAttribs[1].offset   = offsetof(GlyphQuadVertex, uv);

    vk::PipelineVertexInputStateCreateInfo sdfVtxInfo;
    sdfVtxInfo.vertexBindingDescriptionCount   = 1;
    sdfVtxInfo.pVertexBindingDescriptions      = &sdfVtxBinding;
    sdfVtxInfo.vertexAttributeDescriptionCount = 2;
    sdfVtxInfo.pVertexAttributeDescriptions    = sdfAttribs.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
    inputAssembly.topology               = vk::PrimitiveTopology::eTriangleList;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    vk::Viewport viewport;
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(_swapchain.extent().width);
    viewport.height   = static_cast<float>(_swapchain.extent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vk::Rect2D scissor;
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent = _swapchain.extent();

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
    // SDF text quads are flat — disable culling so they are visible regardless of winding order.
    rasterizer.cullMode        = vk::CullModeFlagBits::eNone;
    rasterizer.frontFace       = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = VK_FALSE;

    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    // Alpha blending for SDF text.
    vk::PipelineColorBlendAttachmentState sdfBlend;
    sdfBlend.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    sdfBlend.blendEnable         = VK_TRUE;
    sdfBlend.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    sdfBlend.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    sdfBlend.colorBlendOp        = vk::BlendOp::eAdd;
    sdfBlend.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    sdfBlend.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    sdfBlend.alphaBlendOp        = vk::BlendOp::eAdd;

    vk::PipelineColorBlendStateCreateInfo sdfBlendState;
    sdfBlendState.logicOpEnable   = VK_FALSE;
    sdfBlendState.attachmentCount = 1;
    sdfBlendState.pAttachments    = &sdfBlend;

    // No depth write for text overlay.
    vk::PipelineDepthStencilStateCreateInfo sdfDepth;
    sdfDepth.depthTestEnable  = VK_TRUE;
    sdfDepth.depthWriteEnable = VK_FALSE;
    sdfDepth.depthCompareOp   = vk::CompareOp::eLessOrEqual;

    vk::GraphicsPipelineCreateInfo sdfPipeInfo;
    sdfPipeInfo.stageCount          = static_cast<uint32_t>(sdfStages.size());
    sdfPipeInfo.pStages             = sdfStages.data();
    sdfPipeInfo.pVertexInputState   = &sdfVtxInfo;
    sdfPipeInfo.pInputAssemblyState = &inputAssembly;
    sdfPipeInfo.pViewportState      = &viewportState;
    sdfPipeInfo.pRasterizationState = &rasterizer;
    sdfPipeInfo.pMultisampleState   = &multisampling;
    sdfPipeInfo.pDepthStencilState  = &sdfDepth;
    sdfPipeInfo.pColorBlendState    = &sdfBlendState;
    sdfPipeInfo.layout              = _sdfPipelineLayout.get();
    sdfPipeInfo.renderPass          = _swapchain.renderPass();
    sdfPipeInfo.subpass             = 0;

    auto sdfResult =
        _device.device().createGraphicsPipelineUnique(vk::PipelineCache{}, sdfPipeInfo);
    if (sdfResult.result == vk::Result::eSuccess)
        _sdfGlyphPipeline = std::move(sdfResult.value);
    else
        std::cerr << "Failed to create SDF glyph pipeline" << std::endl;
}

bool VulkanFrameRenderer::createSdfPipeline()
{
    auto sdfGlyphVertCode =
        loadBinaryFile({"build/bin/shaders/glyph_sdf.vert.spv", "bin/shaders/glyph_sdf.vert.spv",
                        "shaders/glyph_sdf.vert.spv"});
    auto sdfGlyphFragCode =
        loadBinaryFile({"build/bin/shaders/glyph_sdf.frag.spv", "bin/shaders/glyph_sdf.frag.spv",
                        "shaders/glyph_sdf.frag.spv"});
    if (sdfGlyphVertCode.empty() || sdfGlyphFragCode.empty())
        return false;

    createSdfDescriptorResources();

    // Pipeline layout: push constants + 1 descriptor set.
    vk::PushConstantRange pushConstantRange;
    pushConstantRange.stageFlags =
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    pushConstantRange.offset = 0;
    pushConstantRange.size   = sizeof(PushConstants);

    vk::PipelineLayoutCreateInfo sdfLayoutInfo;
    auto setLayout                       = _sdfDescriptorSetLayout.get();
    sdfLayoutInfo.setLayoutCount         = 1;
    sdfLayoutInfo.pSetLayouts            = &setLayout;
    sdfLayoutInfo.pushConstantRangeCount = 1;
    sdfLayoutInfo.pPushConstantRanges    = &pushConstantRange;
    _sdfPipelineLayout = _device.device().createPipelineLayoutUnique(sdfLayoutInfo);

    createSdfGraphicsPipelineObject(sdfGlyphVertCode, sdfGlyphFragCode);
    return true;
}

// ---------------------------------------------------------------------------
// SDF data setters
// ---------------------------------------------------------------------------

void VulkanFrameRenderer::setSdfGlyphData(std::vector<GlyphQuadVertex> vertices,
                                          TextureHandle atlasHandle)
{
    _sdfGlyphVertices = std::move(vertices);
    std::fill(_sdfGlyphUploadNeeded.begin(), _sdfGlyphUploadNeeded.end(), true);

    // Update the SDF descriptor set when a new atlas handle is provided.
    if (!atlasHandle.isValid() || atlasHandle.value == _sdfAtlasHandle.value)
        return;

    _sdfAtlasHandle = atlasHandle;

    if (!_sdfDescriptorSetLayout || !_sdfDescriptorPool || !_device.device())
        return;

    vk::DescriptorImageInfo imageInfo;
    imageInfo.sampler     = _textureStore.sampler(_sdfAtlasHandle);
    imageInfo.imageView   = _textureStore.imageView(_sdfAtlasHandle);
    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    vk::WriteDescriptorSet write;
    write.dstSet          = _sdfDescriptorSet;
    write.dstBinding      = 0;
    write.descriptorCount = 1;
    write.descriptorType  = vk::DescriptorType::eCombinedImageSampler;
    write.pImageInfo      = &imageInfo;
    _device.device().updateDescriptorSets(1, &write, 0, nullptr);
}

void VulkanFrameRenderer::setEntityDrawGroups(std::vector<EntityDrawGroup> groups)
{
    _entityDrawGroups = std::move(groups);
}

// ---------------------------------------------------------------------------
// Frame recording
// ---------------------------------------------------------------------------

void VulkanFrameRenderer::recordCommandBuffer(vk::CommandBuffer cmd, uint32_t imageIndex,
                                              const glm::mat4 &viewProjection)
{
    // Time delta + rotation update.
    const auto now     = std::chrono::steady_clock::now();
    float deltaSeconds = 0.0f;
    if (_lastFrameTime.time_since_epoch().count() != 0)
        deltaSeconds = std::chrono::duration<float>(now - _lastFrameTime).count();
    _lastFrameTime                           = now;
    deltaSeconds                             = std::clamp(deltaSeconds, 0.0f, 0.1f);
    constexpr float kRotationSpeedRadPerSec  = 0.75f;
    _demoRotationAngle                      += kRotationSpeedRadPerSec * deltaSeconds;

    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmd.begin(beginInfo);

    const bool isInitialized = _swapchain.imageInitialized(imageIndex);

    vk::ImageMemoryBarrier toColorAttachment;
    toColorAttachment.oldLayout =
        isInitialized ? vk::ImageLayout::ePresentSrcKHR : vk::ImageLayout::eUndefined;
    toColorAttachment.newLayout                       = vk::ImageLayout::eColorAttachmentOptimal;
    toColorAttachment.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    toColorAttachment.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    toColorAttachment.image                           = _swapchain.images()[imageIndex];
    toColorAttachment.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
    toColorAttachment.subresourceRange.baseMipLevel   = 0;
    toColorAttachment.subresourceRange.levelCount     = 1;
    toColorAttachment.subresourceRange.baseArrayLayer = 0;
    toColorAttachment.subresourceRange.layerCount     = 1;
    toColorAttachment.srcAccessMask =
        isInitialized ? vk::AccessFlagBits::eMemoryRead : vk::AccessFlags{};
    toColorAttachment.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

    cmd.pipelineBarrier(isInitialized ? vk::PipelineStageFlagBits::eBottomOfPipe
                                      : vk::PipelineStageFlagBits::eTopOfPipe,
                        vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, 0, nullptr, 0,
                        nullptr, 1, &toColorAttachment);

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
    renderPassInfo.renderPass          = _swapchain.renderPass();
    renderPassInfo.framebuffer         = _swapchain.framebuffers()[imageIndex].get();
    renderPassInfo.renderArea.offset.x = 0;
    renderPassInfo.renderArea.offset.y = 0;
    renderPassInfo.renderArea.extent   = _swapchain.extent();
    renderPassInfo.clearValueCount     = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues        = clearValues.data();

    cmd.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

    const float aspect = static_cast<float>(_swapchain.extent().width) /
                         static_cast<float>((std::max)(_swapchain.extent().height, 1u));

    PushConstants pushConstants{};
    pushConstants.viewProjection        = viewProjection;
    pushConstants.animationData         = glm::vec4(_demoRotationAngle, 0.0f, aspect, 0.0f);
    pushConstants.sunDirectionIntensity = glm::vec4(sunlightDirection, 4.5f);
    pushConstants.lightingParams        = glm::vec4(0.06f, 0.94f, 0.0f, 0.0f);
    pushConstants.modelMatrix           = glm::mat4(1.0f);

    // Telemetry: count drawn entity vertices.
    uint64_t entityVertexCount = 0;
    for (const auto &group : _entityDrawGroups)
        entityVertexCount += static_cast<uint64_t>(group.proceduralVertexCount) *
                             static_cast<uint64_t>(group.modelMatrices.size());
    _lastRenderedVertexCount = entityVertexCount;

    recordEntityDrawCalls(cmd, imageIndex, pushConstants);
    recordSdfDrawCalls(cmd, imageIndex, pushConstants);

    cmd.endRenderPass();

    vk::ImageMemoryBarrier toPresent;
    toPresent.oldLayout                       = vk::ImageLayout::eColorAttachmentOptimal;
    toPresent.newLayout                       = vk::ImageLayout::ePresentSrcKHR;
    toPresent.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    toPresent.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    toPresent.image                           = _swapchain.images()[imageIndex];
    toPresent.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
    toPresent.subresourceRange.baseMipLevel   = 0;
    toPresent.subresourceRange.levelCount     = 1;
    toPresent.subresourceRange.baseArrayLayer = 0;
    toPresent.subresourceRange.layerCount     = 1;
    toPresent.srcAccessMask                   = vk::AccessFlagBits::eColorAttachmentWrite;
    toPresent.dstAccessMask                   = vk::AccessFlagBits::eMemoryRead;

    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                        vk::PipelineStageFlagBits::eBottomOfPipe, {}, 0, nullptr, 0, nullptr, 1,
                        &toPresent);
    cmd.end();
}

void VulkanFrameRenderer::recordEntityDrawCalls(vk::CommandBuffer cmd, uint32_t imageIndex,
                                                const PushConstants &pushConstants)
{
    PushConstants pc = pushConstants;
    for (const auto &group : _entityDrawGroups)
    {
        if (group.modelMatrices.empty() || !group.pipeline.isValid())
            continue;

        auto pipeIt = _pipelineStore.pipeline(group.pipeline);
        if (!pipeIt)
            continue;

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeIt);

        const vk::PipelineLayout texLayout = _textureStore.entityTexturePipelineLayout();
        const bool hasTexture              = group.textureHandle.isValid() && texLayout;
        vk::PipelineLayout groupLayout     = hasTexture ? texLayout : _swapchain.pipelineLayout();

        if (hasTexture)
        {
            auto texDs = _textureStore.entityTextureDescriptorSet(group.textureHandle);
            if (texDs)
            {
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, groupLayout, 0, 1, &texDs,
                                       0, nullptr);
            }
        }

        if (group.instancedRendering)
        {
            const auto instanceCount    = static_cast<uint32_t>(group.modelMatrices.size());
            const vk::DeviceSize needed = sizeof(glm::mat4) * instanceCount;
            const uint64_t key          = group.pipeline.value;

            auto &states                = _instancedBufferStates[key];
            if (states.size() <= imageIndex)
                states.resize(imageIndex + 1);
            auto &buf = states[imageIndex];

            if (needed > buf.capacity)
            {
                buf.buffer.reset();
                buf.memory.reset();

                vk::BufferCreateInfo bufInfo;
                bufInfo.size        = needed;
                bufInfo.usage       = vk::BufferUsageFlagBits::eVertexBuffer;
                bufInfo.sharingMode = vk::SharingMode::eExclusive;
                buf.buffer          = _device.device().createBufferUnique(bufInfo);

                auto memReq = _device.device().getBufferMemoryRequirements(buf.buffer.get());
                vk::MemoryAllocateInfo allocInfo;
                allocInfo.allocationSize  = memReq.size;
                allocInfo.memoryTypeIndex = _device.findMemoryType(
                    memReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible |
                                               vk::MemoryPropertyFlagBits::eHostCoherent);
                buf.memory = _device.device().allocateMemoryUnique(allocInfo);
                _device.device().bindBufferMemory(buf.buffer.get(), buf.memory.get(), 0);
                buf.capacity = needed;
            }

            buf.upload(_device.device(), group.modelMatrices.data(), needed);

            cmd.pushConstants(groupLayout,
                              vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                              0, sizeof(PushConstants), &pc);
            const vk::DeviceSize offset = 0;
            cmd.bindVertexBuffers(0, {buf.buffer.get()}, {offset});
            cmd.draw(group.proceduralVertexCount, instanceCount, 0, 0);
        } else
        {
            for (const auto &modelMatrix : group.modelMatrices)
            {
                pc.modelMatrix = modelMatrix;
                cmd.pushConstants(groupLayout,
                                  vk::ShaderStageFlagBits::eVertex |
                                      vk::ShaderStageFlagBits::eFragment,
                                  0, sizeof(PushConstants), &pc);
                cmd.draw(group.proceduralVertexCount, 1, 0, 0);
            }
        }
    }
}

void VulkanFrameRenderer::recordSdfDrawCalls(vk::CommandBuffer cmd, uint32_t imageIndex,
                                             PushConstants pushConstants)
{
    const auto sdfVertCount = static_cast<uint32_t>(_sdfGlyphVertices.size());
    if (sdfVertCount == 0 || !_sdfGlyphPipeline || !_sdfAtlasHandle.isValid())
        return;

    if (_sdfGlyphBuffers.size() <= imageIndex)
    {
        _sdfGlyphBuffers.resize(imageIndex + 1);
        _sdfGlyphUploadNeeded.resize(imageIndex + 1, true);
    }

    const vk::DeviceSize sdfNeeded = sizeof(GlyphQuadVertex) * sdfVertCount;
    auto &buf                      = _sdfGlyphBuffers[imageIndex];

    if (sdfNeeded > buf.capacity)
    {
        buf.buffer.reset();
        buf.memory.reset();

        vk::BufferCreateInfo sdfBufInfo;
        sdfBufInfo.size        = sdfNeeded;
        sdfBufInfo.usage       = vk::BufferUsageFlagBits::eVertexBuffer;
        sdfBufInfo.sharingMode = vk::SharingMode::eExclusive;
        buf.buffer             = _device.device().createBufferUnique(sdfBufInfo);

        auto sdfMemReq         = _device.device().getBufferMemoryRequirements(buf.buffer.get());
        vk::MemoryAllocateInfo sdfAllocInfo;
        sdfAllocInfo.allocationSize  = sdfMemReq.size;
        sdfAllocInfo.memoryTypeIndex = _device.findMemoryType(
            sdfMemReq.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        buf.memory = _device.device().allocateMemoryUnique(sdfAllocInfo);
        _device.device().bindBufferMemory(buf.buffer.get(), buf.memory.get(), 0);
        buf.capacity                      = sdfNeeded;
        _sdfGlyphUploadNeeded[imageIndex] = true;
    }

    if (_sdfGlyphUploadNeeded[imageIndex])
    {
        buf.upload(_device.device(), _sdfGlyphVertices.data(), sdfNeeded);
        _sdfGlyphUploadNeeded[imageIndex] = false;
    }

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _sdfGlyphPipeline.get());
    pushConstants.modelMatrix = glm::mat4(1.0f);
    // animationData.z/w = clipYMin/clipYMax in world Y (0 disables clip).
    pushConstants.animationData.z = _sdfClipYMin;
    pushConstants.animationData.w = _sdfClipYMax;
    cmd.pushConstants(_sdfPipelineLayout.get(),
                      vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
                      sizeof(PushConstants), &pushConstants);
    auto sdfDescSet = _sdfDescriptorSet;
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _sdfPipelineLayout.get(), 0, 1,
                           &sdfDescSet, 0, nullptr);
    const vk::DeviceSize sdfOffset = 0;
    cmd.bindVertexBuffers(0, {buf.buffer.get()}, {sdfOffset});
    cmd.draw(sdfVertCount, 1, 0, 0);
}

#include "vulkandevice.h"
#include "vulkanpipelinestore.h"

#include <array>
#include <iostream>
#include <vector>
#include <vulkan/vulkan.hpp>

using namespace vigine::ecs::graphics;

namespace
{

vk::Format toVkFormat(VertexFormat f)
{
    switch (f)
    {
    case VertexFormat::Float32:
        return vk::Format::eR32Sfloat;
    case VertexFormat::Float32x2:
        return vk::Format::eR32G32Sfloat;
    case VertexFormat::Float32x3:
        return vk::Format::eR32G32B32Sfloat;
    case VertexFormat::Float32x4:
        return vk::Format::eR32G32B32A32Sfloat;
    case VertexFormat::UInt32:
        return vk::Format::eR32Uint;
    default:
        return vk::Format::eR32G32B32Sfloat;
    }
}

} // namespace

VulkanPipelineStore::VulkanPipelineStore(VulkanDevice &device) : _device(device) {}

VulkanPipelineStore::~VulkanPipelineStore()
{
    for (auto &[key, pipeline] : _pipelineHandles)
        _device.device().destroyPipeline(pipeline);
    _pipelineHandles.clear();

    for (auto &[key, mod] : _shaderModuleHandles)
        _device.device().destroyShaderModule(mod);
    _shaderModuleHandles.clear();
}

void VulkanPipelineStore::buildVertexInputState(
    const PipelineDesc &desc, std::vector<vk::VertexInputBindingDescription> &bindings,
    std::vector<vk::VertexInputAttributeDescription> &attribs) const
{
    for (const auto &vb : desc.vertexLayout)
    {
        vk::VertexInputBindingDescription bd;
        bd.binding = vb.binding;
        bd.stride  = vb.stride;
        bd.inputRate =
            vb.instanceRate ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex;
        bindings.push_back(bd);

        for (const auto &attr : vb.attributes)
        {
            vk::VertexInputAttributeDescription ad;
            ad.binding  = vb.binding;
            ad.location = attr.location;
            ad.format   = toVkFormat(attr.format);
            ad.offset   = attr.offset;
            attribs.push_back(ad);
        }
    }
}

PipelineHandle VulkanPipelineStore::createPipeline(const PipelineDesc &desc,
                                                   vk::RenderPass renderPass,
                                                   vk::Extent2D swapchainExtent,
                                                   vk::PipelineLayout defaultLayout,
                                                   vk::PipelineLayout textureLayout)
{
    // Select pipeline layout based on whether the shader uses a texture sampler.
    const bool useTexLayout         = desc.hasTextureBinding && textureLayout;
    vk::PipelineLayout chosenLayout = useTexLayout ? textureLayout : defaultLayout;

    // Resolve Vulkan shader modules from handles.
    auto vertIt = _shaderModuleHandles.find(desc.vertexShader.value);
    auto fragIt = _shaderModuleHandles.find(desc.fragmentShader.value);
    if (vertIt == _shaderModuleHandles.end() || fragIt == _shaderModuleHandles.end())
        return {};

    vk::PipelineShaderStageCreateInfo vertStage;
    vertStage.stage  = vk::ShaderStageFlagBits::eVertex;
    vertStage.module = vertIt->second;
    vertStage.pName  = "main";

    vk::PipelineShaderStageCreateInfo fragStage;
    fragStage.stage                                         = vk::ShaderStageFlagBits::eFragment;
    fragStage.module                                        = fragIt->second;
    fragStage.pName                                         = "main";

    std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {vertStage, fragStage};

    // Vertex input.
    std::vector<vk::VertexInputBindingDescription> bindings;
    std::vector<vk::VertexInputAttributeDescription> attribs;
    buildVertexInputState(desc, bindings, attribs);

    vk::PipelineVertexInputStateCreateInfo vertexInput;
    vertexInput.vertexBindingDescriptionCount   = static_cast<uint32_t>(bindings.size());
    vertexInput.pVertexBindingDescriptions      = bindings.data();
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribs.size());
    vertexInput.pVertexAttributeDescriptions    = attribs.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
    inputAssembly.topology               = (desc.topology == Topology::LineList)
                                               ? vk::PrimitiveTopology::eLineList
                                               : vk::PrimitiveTopology::eTriangleList;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    vk::Viewport viewport;
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(swapchainExtent.width);
    viewport.height   = static_cast<float>(swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vk::Rect2D scissor;
    scissor.offset = vk::Offset2D{0, 0};
    scissor.extent = swapchainExtent;

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
    rasterizer.frontFace               = vk::FrontFace::eClockwise;
    rasterizer.depthBiasEnable         = VK_FALSE;

    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineDepthStencilStateCreateInfo depthStencil;
    depthStencil.depthTestEnable  = desc.depthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = desc.depthWrite ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp   = vk::CompareOp::eLessOrEqual;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment;
    colorBlendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    if (desc.blendMode == BlendMode::AlphaBlend)
    {
        colorBlendAttachment.blendEnable         = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
        colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        colorBlendAttachment.colorBlendOp        = vk::BlendOp::eAdd;
        colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
        colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
        colorBlendAttachment.alphaBlendOp        = vk::BlendOp::eAdd;
    } else
    {
        colorBlendAttachment.blendEnable = VK_FALSE;
    }

    vk::PipelineColorBlendStateCreateInfo colorBlendState;
    colorBlendState.logicOpEnable   = VK_FALSE;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments    = &colorBlendAttachment;

    vk::GraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.stageCount          = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages             = stages.data();
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlendState;
    pipelineInfo.layout              = chosenLayout;
    pipelineInfo.renderPass          = renderPass;
    pipelineInfo.subpass             = 0;

    auto result = _device.device().createGraphicsPipeline(vk::PipelineCache{}, pipelineInfo);
    if (result.result != vk::Result::eSuccess)
    {
        std::cerr << "VulkanPipelineStore::createPipeline failed" << std::endl;
        return {};
    }

    PipelineHandle handle;
    handle.value                   = _nextPipelineHandleId++;
    _pipelineHandles[handle.value] = result.value;
    return handle;
}

void VulkanPipelineStore::destroyPipeline(PipelineHandle handle)
{
    auto it = _pipelineHandles.find(handle.value);
    if (it != _pipelineHandles.end())
    {
        _device.device().destroyPipeline(it->second);
        _pipelineHandles.erase(it);
    }
}

ShaderModuleHandle VulkanPipelineStore::createShaderModule(const std::vector<char> &spirv)
{
    if (spirv.empty() || !_device.device())
        return {};

    vk::ShaderModuleCreateInfo createInfo;
    createInfo.codeSize  = spirv.size();
    createInfo.pCode     = reinterpret_cast<const uint32_t *>(spirv.data());

    vk::ShaderModule mod = _device.device().createShaderModule(createInfo);

    ShaderModuleHandle handle;
    handle.value                       = _nextPipelineHandleId++;
    _shaderModuleHandles[handle.value] = mod;
    return handle;
}

void VulkanPipelineStore::destroyShaderModule(ShaderModuleHandle handle)
{
    auto it = _shaderModuleHandles.find(handle.value);
    if (it != _shaderModuleHandles.end())
    {
        _device.device().destroyShaderModule(it->second);
        _shaderModuleHandles.erase(it);
    }
}

vk::Pipeline VulkanPipelineStore::pipeline(PipelineHandle handle) const
{
    auto it = _pipelineHandles.find(handle.value);
    return it != _pipelineHandles.end() ? it->second : vk::Pipeline{};
}

vk::ShaderModule VulkanPipelineStore::shaderModule(ShaderModuleHandle handle) const
{
    auto it = _shaderModuleHandles.find(handle.value);
    return it != _shaderModuleHandles.end() ? it->second : vk::ShaderModule{};
}

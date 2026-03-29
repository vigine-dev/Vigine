#pragma once

#include "vigine/ecs/render/graphicsbackend.h"
#include "vigine/ecs/render/graphicshandles.h"

#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace vigine
{
namespace graphics
{

class VulkanDevice;

class VulkanPipelineStore
{
  public:
    explicit VulkanPipelineStore(VulkanDevice &device);
    ~VulkanPipelineStore();

    // Pipeline CRUD.
    // renderPass, swapchainExtent, defaultLayout, textureLayout are swapchain-scoped
    // (re-passed on each call after swapchain recreation).
    PipelineHandle createPipeline(const PipelineDesc &desc, vk::RenderPass renderPass,
                                  vk::Extent2D swapchainExtent, vk::PipelineLayout defaultLayout,
                                  vk::PipelineLayout textureLayout);
    void destroyPipeline(PipelineHandle handle);

    // Shader module CRUD.
    ShaderModuleHandle createShaderModule(const std::vector<char> &spirv);
    void destroyShaderModule(ShaderModuleHandle handle);

    // Accessors.
    // Returns a null pipeline if handle is unknown.
    vk::Pipeline pipeline(PipelineHandle handle) const;
    // Returns a null shader module if handle is unknown.
    vk::ShaderModule shaderModule(ShaderModuleHandle handle) const;

  private:
    void buildVertexInputState(const PipelineDesc &desc,
                               std::vector<vk::VertexInputBindingDescription> &bindings,
                               std::vector<vk::VertexInputAttributeDescription> &attribs) const;

    VulkanDevice &_device;
    uint64_t _nextPipelineHandleId{1};

    std::unordered_map<uint64_t, vk::Pipeline> _pipelineHandles;
    std::unordered_map<uint64_t, vk::ShaderModule> _shaderModuleHandles;
};

} // namespace graphics
} // namespace vigine

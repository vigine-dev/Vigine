#pragma once

#include "vigine/ecs/render/graphicshandles.h"

#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace vigine
{
namespace graphics
{

class VulkanDevice;

class VulkanTextureStore
{
  public:
    explicit VulkanTextureStore(VulkanDevice &device);
    ~VulkanTextureStore();

    // Called once after device creation (device-scoped, survives swapchain recreation).
    void initEntityTextureDescriptorResources(uint32_t pushConstantSize);

    // Texture CRUD.
    TextureHandle createTexture(const TextureDesc &desc);
    void uploadTexture(TextureHandle handle, const void *pixels, uint32_t width, uint32_t height);
    void destroyTexture(TextureHandle handle);

    // Descriptor set for a given texture (must call createTextureDescriptorSet first).
    void createTextureDescriptorSet(TextureHandle handle);

    // Remove completed staging uploads whose GPU fence has signaled.
    void cleanupCompletedTextureUploads();

    // Accessors for external consumers (VulkanAPI, future VulkanFrameRenderer).
    vk::ImageView imageView(TextureHandle handle) const;
    vk::Sampler sampler(TextureHandle handle) const;
    vk::PipelineLayout entityTexturePipelineLayout() const
    {
        return _entityTexturePipelineLayout.get();
    }
    // Returns a null descriptor set if handle is unknown.
    vk::DescriptorSet entityTextureDescriptorSet(TextureHandle handle) const;

  private:
    void recordImageLayoutTransitions(vk::CommandBuffer cmd, vk::Buffer stagingBuffer,
                                      vk::Image image, uint32_t width, uint32_t height);

    VulkanDevice &_device;
    uint64_t _nextTextureHandleId{1};

    std::unordered_map<uint64_t, vk::Image> _textureHandles;
    std::unordered_map<uint64_t, vk::DeviceMemory> _textureMemoryHandles;
    std::unordered_map<uint64_t, vk::ImageView> _textureViewHandles;
    std::unordered_map<uint64_t, vk::Sampler> _textureSamplerHandles;
    std::unordered_map<uint64_t, TextureDesc> _textureDescs;

    vk::UniqueDescriptorSetLayout _entityTextureDescriptorSetLayout;
    vk::UniqueDescriptorPool _entityTextureDescriptorPool;
    vk::UniquePipelineLayout _entityTexturePipelineLayout;
    std::unordered_map<uint64_t, vk::DescriptorSet> _textureDescriptorSets;

    struct TextureUploadStaging
    {
        vk::UniqueBuffer buffer;
        vk::UniqueDeviceMemory memory;
        vk::UniqueFence fence;
        vk::UniqueCommandPool commandPool;
    };
    std::vector<TextureUploadStaging> _pendingTextureUploads;
};

} // namespace graphics
} // namespace vigine

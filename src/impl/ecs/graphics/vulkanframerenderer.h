#pragma once

#include "vulkantypes.h"

#include "vigine/impl/ecs/graphics/graphicshandles.h"
#include "vigine/impl/ecs/graphics/textcomponent.h"

#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace vigine
{
namespace ecs
{
namespace graphics
{

class VulkanDevice;
class VulkanSwapchain;
class VulkanTextureStore;
class VulkanPipelineStore;

// Owns frame-level rendering state: per-frame CPU→GPU vertex buffers, SDF pipeline,
// and all draw-call recording logic. Called by VulkanAPI::drawFrame().
class VulkanFrameRenderer
{
  public:
    VulkanFrameRenderer(VulkanDevice &device, VulkanSwapchain &swapchain,
                        VulkanTextureStore &textureStore, VulkanPipelineStore &pipelineStore);
    ~VulkanFrameRenderer() = default;

    // Build SDF glyph pipeline (called from createSwapchain after render pass is ready).
    // Returns false if shader SPIR-V is not found — caller should warn and continue.
    bool createSdfPipeline();

    // Destroy per-swapchain-image buffers and SDF pipeline (called before swapchain recreate).
    void cleanupSwapchainResources();

    // Record a complete frame into cmd, including entity and SDF draw calls.
    // Updates internal rotation angle and timing state.
    void recordCommandBuffer(vk::CommandBuffer cmd, uint32_t imageIndex,
                             const glm::mat4 &viewProjection);

    // Entity draw groups (set by RenderSystem each frame via VulkanAPI delegation).
    void setEntityDrawGroups(std::vector<EntityDrawGroup> groups);

    // SDF glyph vertex data + atlas texture. Called by RenderSystem when text changes.
    void setSdfGlyphData(std::vector<GlyphQuadVertex> vertices, TextureHandle atlasHandle);

    // SDF clip planes (world Y). clipYMax == 0 → no clipping.
    void setSdfClipY(float yMin, float yMax)
    {
        _sdfClipYMin = yMin;
        _sdfClipYMax = yMax;
    }

    [[nodiscard]] uint64_t lastRenderedVertexCount() const { return _lastRenderedVertexCount; }

  private:
    // Host-visible per-swapchain-image vertex buffer that grows on demand.
    struct VulkanPerImageBuffer
    {
        vk::UniqueBuffer buffer;
        vk::UniqueDeviceMemory memory;
        vk::DeviceSize capacity{0};

        bool ensureCapacity(vk::Device device, uint32_t memTypeIndex, vk::DeviceSize needed);
        void upload(vk::Device device, const void *src, vk::DeviceSize size);
    };

    void createSdfDescriptorResources();
    void createSdfGraphicsPipelineObject(const std::vector<char> &vertCode,
                                         const std::vector<char> &fragCode);

    void recordEntityDrawCalls(vk::CommandBuffer cmd, uint32_t imageIndex,
                               const PushConstants &pushConstants);
    void recordSdfDrawCalls(vk::CommandBuffer cmd, uint32_t imageIndex,
                            PushConstants pushConstants);

    VulkanDevice &_device;
    VulkanSwapchain &_swapchain;
    VulkanTextureStore &_textureStore;
    VulkanPipelineStore &_pipelineStore;

    // Entity instance buffers per pipeline key, per swapchain image.
    std::unordered_map<uint64_t, std::vector<VulkanPerImageBuffer>> _instancedBufferStates;

    // ECS draw groups delivered by RenderSystem.
    std::vector<EntityDrawGroup> _entityDrawGroups;

    // SDF glyph geometry (flat vertex list, one buffer per swapchain image).
    std::vector<GlyphQuadVertex> _sdfGlyphVertices;
    std::vector<bool> _sdfGlyphUploadNeeded;
    std::vector<VulkanPerImageBuffer> _sdfGlyphBuffers;

    // SDF font atlas (GPU handle; ownership belongs to RenderSystem via TextureComponent).
    TextureHandle _sdfAtlasHandle;
    vk::UniqueDescriptorSetLayout _sdfDescriptorSetLayout;
    vk::UniqueDescriptorPool _sdfDescriptorPool;
    vk::DescriptorSet _sdfDescriptorSet;
    vk::UniquePipelineLayout _sdfPipelineLayout;
    vk::UniquePipeline _sdfGlyphPipeline;

    float _sdfClipYMin{0.f};
    float _sdfClipYMax{0.f};

    float _demoRotationAngle{0.0f};
    std::chrono::steady_clock::time_point _lastFrameTime{};
    uint64_t _lastRenderedVertexCount{0};
};

} // namespace graphics
} // namespace ecs
} // namespace vigine

#pragma once

/**
 * @file vulkanapi.h
 * @brief Vulkan implementation of the GraphicsBackend interface.
 */

#include "graphicsbackend.h"

#include "vigine/base/macros.h"
#include "vigine/ecs/render/textcomponent.h"

#include <cstddef>
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace vigine
{
namespace graphics
{

class VulkanDevice;
class VulkanSwapchain;
class VulkanTextureStore;
class VulkanPipelineStore;
class VulkanFrameRenderer;

/**
 * @brief Vulkan-backed GraphicsBackend.
 *
 * Owns the Vulkan instance, physical and logical devices, surface,
 * swapchain, and frame renderer. Implements the GraphicsBackend
 * contract (pipeline / buffer / texture / shader creation, push
 * constants, frame begin / end, draw submit) and adds Vulkan-specific
 * helpers: setEntityDrawGroups, setSdfGlyphData, setSdfClipY,
 * descriptor-set creation, and raw vk::Instance / Device accessors
 * used during gradual refactoring.
 */
class VulkanAPI : public GraphicsBackend
{
  public:
    VulkanAPI();
    ~VulkanAPI();

    // Initialization
    bool initializeInstance();
    bool selectPhysicalDevice();
    bool createLogicalDevice();
    bool createSurface(void *nativeWindowHandle);
    bool createSwapchain(uint32_t width, uint32_t height);
    bool recreateSwapchain(uint32_t width, uint32_t height);
    bool drawFrame(const glm::mat4 &viewProjection);
    void setEntityDrawGroups(std::vector<EntityDrawGroup> groups);
    // SDF glyph rendering: flat vertex list + atlas handle.
    // Atlas GPU texture is managed by RenderSystem via TextureComponent.
    void setSdfGlyphData(std::vector<GlyphQuadVertex> vertices, TextureHandle atlasHandle);
    [[nodiscard]] uint64_t lastRenderedVertexCount() const;
    [[nodiscard]] uint32_t swapchainGeneration() const;
    [[nodiscard]] uint32_t swapchainWidth() const;
    [[nodiscard]] uint32_t swapchainHeight() const;

    // SDF text clip planes (world Y). clipYMax == 0 → no clipping.
    void setSdfClipY(float yMin, float yMax);

    // Vulkan resources access
    vk::Instance getInstance() const;
    vk::PhysicalDevice getPhysicalDevice() const;
    vk::Device getLogicalDevice() const;
    vk::Queue getGraphicsQueue() const;

    // Texture Vulkan resource access (for descriptor set creation)
    vk::ImageView getTextureImageView(TextureHandle handle) const;
    vk::Sampler getTextureSampler(TextureHandle handle) const;

    // Allocate and configure a Vulkan descriptor set for the given GPU texture.
    // Stores it internally; looked up by TextureHandle.value during drawFrame.
    void createTextureDescriptorSet(TextureHandle handle);

    // Utility
    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
    bool isInitialized() const;
    bool hasSwapchain() const;
    void cleanupCompletedTextureUploads();

    // GraphicsBackend interface implementation
    bool initializeDevice(void *nativeWindow) override;
    bool resize(uint32_t width, uint32_t height) override;
    bool beginFrame() override;
    bool endFrame() override;
    void submitDrawCall(const DrawCallDesc &desc) override;
    PipelineHandle createPipeline(const PipelineDesc &desc) override;
    void destroyPipeline(PipelineHandle handle) override;
    BufferHandle createBuffer(const BufferDesc &desc) override;
    void uploadBuffer(BufferHandle handle, const void *data, size_t size) override;
    void destroyBuffer(BufferHandle handle) override;
    TextureHandle createTexture(const TextureDesc &desc) override;
    void uploadTexture(TextureHandle handle, const void *pixels, uint32_t width,
                       uint32_t height) override;
    void destroyTexture(TextureHandle handle) override;
    ShaderModuleHandle createShaderModule(const std::vector<char> &spirv) override;
    void destroyShaderModule(ShaderModuleHandle handle) override;
    void setViewProjection(const glm::mat4 &viewProjection) override;
    void setPushConstants(const PushConstantData &data) override;

  private:
    // Handle management (buffers only; pipeline/shader handles are in VulkanPipelineStore)
    uint64_t _nextHandleId{1};
    std::unordered_map<uint64_t, vk::Buffer> _bufferHandles;
    std::unordered_map<uint64_t, vk::DeviceMemory> _bufferMemoryHandles;

    void cleanupSwapchainResources();
    bool recreateSwapchainFromSurfaceExtent();

    // GraphicsBackend state
    glm::mat4 _currentViewProjection{1.0f};
    PushConstantData _currentPushConstants{};

    // Vulkan helper objects
    std::unique_ptr<VulkanDevice> _vulkanDevice;
    std::unique_ptr<VulkanSwapchain> _vulkanSwapchain;
    std::unique_ptr<VulkanTextureStore> _vulkanTextureStore;
    std::unique_ptr<VulkanPipelineStore> _vulkanPipelineStore;
    std::unique_ptr<VulkanFrameRenderer> _vulkanFrameRenderer;
};

using VulkanAPIUPtr = std::unique_ptr<VulkanAPI>;
using VulkanAPISPtr = std::shared_ptr<VulkanAPI>;

} // namespace graphics
} // namespace vigine

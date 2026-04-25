#pragma once

/**
 * @file vulkanapi.h
 * @brief Vulkan implementation of the GraphicsBackend interface.
 *
 * Public surface stays Vulkan-SDK-free: no Vulkan SDK headers are
 * pulled in transitively, no `vk::*` types appear in member or
 * function signatures. Backend-specific state (Vulkan handles, draw
 * resources, helper objects) lives in the .cpp behind a forward-
 * declared `Impl` PIMPL plus opaque `unique_ptr` members for the
 * helper sub-systems.
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
#include <vector>

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
 * helpers: setEntityDrawGroups, setSdfGlyphData, setSdfClipY, and
 * descriptor-set creation. Vulkan handle plumbing (vk::Buffer /
 * vk::DeviceMemory bookkeeping, the next-handle counter) is hidden in
 * a PIMPL `Impl` so callers never need the Vulkan SDK on their include
 * path.
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

    // Allocate and configure a Vulkan descriptor set for the given GPU texture.
    // Stores it internally; looked up by TextureHandle.value during drawFrame.
    void createTextureDescriptorSet(TextureHandle handle);

    // Utility
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
    void cleanupSwapchainResources();
    bool recreateSwapchainFromSurfaceExtent();

    // GraphicsBackend state
    glm::mat4 _currentViewProjection{1.0f};
    PushConstantData _currentPushConstants{};

    // Vulkan helper objects -- opaque to public callers; full types
    // visible only inside vulkanapi.cpp via forward declarations above.
    std::unique_ptr<VulkanDevice> _vulkanDevice;
    std::unique_ptr<VulkanSwapchain> _vulkanSwapchain;
    std::unique_ptr<VulkanTextureStore> _vulkanTextureStore;
    std::unique_ptr<VulkanPipelineStore> _vulkanPipelineStore;
    std::unique_ptr<VulkanFrameRenderer> _vulkanFrameRenderer;

    // PIMPL holding everything that requires Vulkan SDK headers
    // (handle counter + vk::Buffer / vk::DeviceMemory maps for buffers
    // owned directly by VulkanAPI). Defined in vulkanapi.cpp.
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

using VulkanAPIUPtr = std::unique_ptr<VulkanAPI>;
using VulkanAPISPtr = std::shared_ptr<VulkanAPI>;

} // namespace graphics
} // namespace vigine

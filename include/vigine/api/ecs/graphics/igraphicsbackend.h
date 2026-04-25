#pragma once

/**
 * @file igraphicsbackend.h
 * @brief Pure-virtual interface for graphics backends used by the ECS render
 *        system. Concrete backends (VulkanAPI today, DirectX / Metal in the
 *        future) implement this contract and live behind the public surface.
 */

#include "vigine/impl/ecs/graphics/graphicshandles.h"

#include <cstddef>
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <vector>

namespace vigine
{
namespace ecs
{
namespace graphics
{

/**
 * @brief Pure-virtual graphics backend interface.
 *
 * Provides a platform-agnostic API for graphics operations.
 * Implementations (e.g., VulkanAPI) handle backend-specific details.
 * This interface supports hot-swapping graphics backends (Vulkan, DirectX, Metal, etc.).
 *
 * @note TODO(#293): A separate @c vigine::platform::IGraphicsBackend
 * lives in @c include/vigine/api/platform/igraphicsbackend.h as the
 * lower-level platform-graphics-context selector (Vulkan vs Metal vs
 * D3D vs WebGPU vs availability-probe). This ECS-side
 * @c vigine::graphics::GraphicsBackend is the rendering surface used
 * by the ECS render system after a platform backend has been picked.
 * A follow-up leaf will reconcile the two layers (delegate /
 * compose / or merge).
 */
class IGraphicsBackend
{
  public:
    /**
     * @brief Virtual destructor to ensure proper cleanup of derived classes.
     */
    virtual ~IGraphicsBackend() = default;

    /**
     * @brief Initialize the graphics device.
     * @param nativeWindow Platform-specific window handle (HWND on Windows, etc.)
     * @return true on success, false on failure
     */
    virtual bool initializeDevice(void *nativeWindow) = 0;

    /**
     * @brief Resize the swapchain/framebuffer.
     * @param width New width in pixels
     * @param height New height in pixels
     * @return true on success, false on failure
     */
    virtual bool resize(uint32_t width, uint32_t height) = 0;

    /**
     * @brief Begin a new frame.
     * @return true if ready to render, false if should skip frame (e.g., minimized)
     */
    virtual bool beginFrame() = 0;

    /**
     * @brief End the current frame and present to screen.
     * @return true on success, false on failure
     */
    virtual bool endFrame() = 0;

    /**
     * @brief Submit a draw call.
     * @param desc Draw call parameters (pipeline, buffers, push constants, etc.)
     */
    virtual void submitDrawCall(const DrawCallDesc &desc) = 0;

    /**
     * @brief Create a graphics pipeline.
     * @param desc Pipeline description (shaders, vertex layout, blend mode, etc.)
     * @return Handle to the created pipeline
     */
    virtual PipelineHandle createPipeline(const PipelineDesc &desc) = 0;

    /**
     * @brief Destroy a pipeline.
     * @param handle Handle to the pipeline to destroy
     */
    virtual void destroyPipeline(PipelineHandle handle) = 0;

    /**
     * @brief Create a buffer.
     * @param desc Buffer description (size, usage, memory flags)
     * @return Handle to the created buffer
     */
    virtual BufferHandle createBuffer(const BufferDesc &desc) = 0;

    /**
     * @brief Upload data to a buffer.
     * @param handle Buffer handle
     * @param data Pointer to source data
     * @param size Size in bytes
     */
    virtual void uploadBuffer(BufferHandle handle, const void *data, size_t size) = 0;

    /**
     * @brief Destroy a buffer.
     * @param handle Handle to the buffer to destroy
     */
    virtual void destroyBuffer(BufferHandle handle) = 0;

    /**
     * @brief Create a texture.
     * @param desc Texture description (width, height, format, sampler params)
     * @return Handle to the created texture
     */
    virtual TextureHandle createTexture(const TextureDesc &desc) = 0;

    /**
     * @brief Upload pixel data to a texture.
     * @param handle Texture handle
     * @param pixels Pointer to pixel data
     * @param width Width in pixels
     * @param height Height in pixels
     */
    virtual void uploadTexture(TextureHandle handle, const void *pixels, uint32_t width,
                               uint32_t height) = 0;

    /**
     * @brief Destroy a texture.
     * @param handle Handle to the texture to destroy
     */
    virtual void destroyTexture(TextureHandle handle) = 0;

    /**
     * @brief Create a shader module from SPIR-V bytecode.
     * @param spirv SPIR-V binary data
     * @return Handle to the created shader module
     */
    virtual ShaderModuleHandle createShaderModule(const std::vector<char> &spirv) = 0;

    /**
     * @brief Destroy a shader module.
     * @param handle Handle to the shader module to destroy
     */
    virtual void destroyShaderModule(ShaderModuleHandle handle) = 0;

    /**
     * @brief Set the view-projection matrix for subsequent draw calls.
     * @param viewProjection Combined view-projection matrix
     */
    virtual void setViewProjection(const glm::mat4 &viewProjection) = 0;

    /**
     * @brief Set push constants for subsequent draw calls.
     * @param data Typed push constant data structure
     */
    virtual void setPushConstants(const PushConstantData &data) = 0;
};

} // namespace graphics
} // namespace ecs
} // namespace vigine

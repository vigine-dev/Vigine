#pragma once

/**
 * @file graphicshandles.h
 * @brief Backend-neutral handle types and descriptors for the graphics API.
 */

#include <cstdint>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vector>

namespace vigine
{
namespace graphics
{

// Opaque handle types wrapping uint64_t
/**
 * @brief Opaque handle identifying a graphics pipeline resource.
 */
struct PipelineHandle
{
    uint64_t value{0};
    bool isValid() const { return value != 0; }
};

/**
 * @brief Opaque handle identifying a GPU buffer resource.
 */
struct BufferHandle
{
    uint64_t value{0};
    bool isValid() const { return value != 0; }
};

/**
 * @brief Opaque handle identifying a GPU texture resource.
 */
struct TextureHandle
{
    uint64_t value{0};
    bool isValid() const { return value != 0; }
};

/**
 * @brief Opaque handle identifying a compiled shader module.
 */
struct ShaderModuleHandle
{
    uint64_t value{0};
    bool isValid() const { return value != 0; }
};

// Vertex layout description
/**
 * @brief Scalar / vector format of a single vertex attribute.
 */
enum class VertexFormat
{
    Float32,
    Float32x2,
    Float32x3,
    Float32x4,
    UInt32
};

/**
 * @brief Single attribute slot inside a vertex binding (location, format, offset).
 */
struct VertexAttribute
{
    uint32_t location;
    VertexFormat format;
    uint32_t offset;
};

/**
 * @brief Vertex binding description: stride, step rate, and attribute list.
 */
struct VertexBindingDesc
{
    uint32_t binding;
    uint32_t stride;
    bool instanceRate{false};
    std::vector<VertexAttribute> attributes;
};

// Pipeline description
/**
 * @brief Blend mode selected by a pipeline (opaque or alpha-blended).
 */
enum class BlendMode
{
    Opaque,
    AlphaBlend
};

/**
 * @brief Primitive topology for rasterisation.
 */
enum class Topology
{
    TriangleList,
    LineList
};

/**
 * @brief Full pipeline description used to create a PipelineHandle.
 */
struct PipelineDesc
{
    ShaderModuleHandle vertexShader;
    ShaderModuleHandle fragmentShader;
    std::vector<VertexBindingDesc> vertexLayout;
    BlendMode blendMode{BlendMode::Opaque};
    bool depthWrite{true};
    bool depthTest{true};
    Topology topology{Topology::TriangleList};
    bool hasTextureBinding{false};
};

// Buffer description
/**
 * @brief Intended GPU usage of a buffer (vertex / index / uniform / storage).
 */
enum class BufferUsage
{
    Vertex,
    Index,
    Uniform,
    Storage
};

/**
 * @brief Memory domain of a buffer (GPU-only, upload, readback).
 */
enum class MemoryUsage
{
    GpuOnly,
    CpuToGpu,
    GpuToCpu
};

/**
 * @brief Buffer description used to create a BufferHandle.
 */
struct BufferDesc
{
    size_t size;
    BufferUsage usage;
    MemoryUsage memoryUsage{MemoryUsage::GpuOnly};
};

// Texture description
/**
 * @brief Pixel format of a texture (colour / depth).
 */
enum class TextureFormat
{
    R8_UNORM,
    RGBA8_UNORM,
    RGBA8_SRGB,
    BGRA8_SRGB,
    D32_SFLOAT
};

/**
 * @brief Sampler filter mode used when sampling a texture.
 */
enum class TextureFilter
{
    Nearest,
    Linear
};

/**
 * @brief Sampler wrap mode applied outside the [0, 1] UV range.
 */
enum class TextureWrapMode
{
    Repeat,
    ClampToEdge,
    ClampToBorder
};

/**
 * @brief Texture description used to create a TextureHandle.
 */
struct TextureDesc
{
    uint32_t width;
    uint32_t height;
    TextureFormat format;
    TextureFilter minFilter{TextureFilter::Linear};
    TextureFilter magFilter{TextureFilter::Linear};
    TextureWrapMode wrapU{TextureWrapMode::Repeat};
    TextureWrapMode wrapV{TextureWrapMode::Repeat};
};

/**
 * @brief Push-constant payload forwarded to shaders every draw call.
 */
struct PushConstantData
{
    glm::mat4 viewProjection;
    glm::vec4 animationData;         // time, frame, etc.
    glm::vec4 sunDirectionIntensity; // xyz = direction, w = intensity
    glm::vec4 lightingParams;        // ambient, diffuse, specular, shininess
    glm::mat4 modelMatrix;
};

/**
 * @brief Description of a single draw call submitted to the backend.
 */
struct DrawCallDesc
{
    PipelineHandle pipeline;
    std::vector<BufferHandle> vertexBuffers;
    BufferHandle indexBuffer;
    PushConstantData pushConstants;
    uint32_t vertexCount{0};
    uint32_t instanceCount{1};
    uint32_t firstVertex{0};
    uint32_t firstInstance{0};
    uint32_t indexCount{0}; // 0 = not using indices
};

/**
 * @brief Per-pipeline batch of entities drawn together by the backend.
 *
 * One EntityDrawGroup per unique pipeline, bundling the texture to
 * bind, the procedural vertex count, the instancing flag, and the
 * per-entity model matrices. Produced by RenderSystem and consumed by
 * VulkanAPI.
 */
struct EntityDrawGroup
{
    PipelineHandle pipeline;
    TextureHandle textureHandle; // valid when shader uses sampler2D (set=0, binding=0)
    uint32_t proceduralVertexCount{0};
    bool instancedRendering{false};
    std::vector<glm::mat4> modelMatrices;
};

} // namespace graphics
} // namespace vigine

#pragma once

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
struct PipelineHandle
{
    uint64_t value{0};
    bool isValid() const { return value != 0; }
};

struct BufferHandle
{
    uint64_t value{0};
    bool isValid() const { return value != 0; }
};

struct TextureHandle
{
    uint64_t value{0};
    bool isValid() const { return value != 0; }
};

struct ShaderModuleHandle
{
    uint64_t value{0};
    bool isValid() const { return value != 0; }
};

// Vertex layout description
enum class VertexFormat
{
    Float32,
    Float32x2,
    Float32x3,
    Float32x4,
    UInt32
};

struct VertexAttribute
{
    uint32_t location;
    VertexFormat format;
    uint32_t offset;
};

struct VertexBindingDesc
{
    uint32_t binding;
    uint32_t stride;
    bool instanceRate{false};
    std::vector<VertexAttribute> attributes;
};

// Pipeline description
enum class BlendMode
{
    Opaque,
    AlphaBlend
};

enum class Topology
{
    TriangleList,
    LineList
};

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
enum class BufferUsage
{
    Vertex,
    Index,
    Uniform,
    Storage
};

enum class MemoryUsage
{
    GpuOnly,
    CpuToGpu,
    GpuToCpu
};

struct BufferDesc
{
    size_t size;
    BufferUsage usage;
    MemoryUsage memoryUsage{MemoryUsage::GpuOnly};
};

// Texture description
enum class TextureFormat
{
    R8_UNORM,
    RGBA8_UNORM,
    RGBA8_SRGB,
    BGRA8_SRGB,
    D32_SFLOAT
};

enum class TextureFilter
{
    Nearest,
    Linear
};

enum class TextureWrapMode
{
    Repeat,
    ClampToEdge,
    ClampToBorder
};

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

// Push constants data structure
struct PushConstantData
{
    glm::mat4 viewProjection;
    glm::vec4 animationData;         // time, frame, etc.
    glm::vec4 sunDirectionIntensity; // xyz = direction, w = intensity
    glm::vec4 lightingParams;        // ambient, diffuse, specular, shininess
    glm::mat4 modelMatrix;
};

// Draw call description
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

// Entity draw group — one group per unique pipeline, used by RenderSystem → VulkanAPI
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

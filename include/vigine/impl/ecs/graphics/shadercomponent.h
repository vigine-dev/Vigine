#pragma once

/**
 * @file shadercomponent.h
 * @brief Per-entity shader configuration: SPIR-V sources and pipeline state.
 */

#include "graphicshandles.h"

#include "vigine/base/macros.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace vigine
{
namespace ecs
{
namespace graphics
{

/**
 * @brief Bundles shader source paths, SPIR-V binaries, and pipeline state.
 *
 * Holds the vertex / fragment shader paths, loads and caches their
 * SPIR-V binaries, and carries every pipeline-relevant setting the
 * PipelineCache needs to key on: vertex layout, blend mode, depth
 * test / write, topology, procedural vertex count, instancing, voxel
 * text layout flag, and texture binding flag.
 */
class ShaderComponent
{
  public:
    ShaderComponent() = default;
    ShaderComponent(const std::string &vertPath, const std::string &fragPath)
        : _vertexShaderPath(vertPath), _fragmentShaderPath(fragPath)
    {
    }

    void setVertexShaderPath(const std::string &path) { _vertexShaderPath = path; }
    void setFragmentShaderPath(const std::string &path) { _fragmentShaderPath = path; }

    const std::string &getVertexShaderPath() const { return _vertexShaderPath; }
    const std::string &getFragmentShaderPath() const { return _fragmentShaderPath; }

    // SPIR-V binary cache
    bool loadSpirv();
    bool spirvLoaded() const { return !_vertexSpirv.empty() && !_fragmentSpirv.empty(); }
    const std::vector<char> &vertexSpirv() const { return _vertexSpirv; }
    const std::vector<char> &fragmentSpirv() const { return _fragmentSpirv; }

    // Vertex layout
    void setVertexLayout(std::vector<VertexBindingDesc> layout)
    {
        _vertexLayout = std::move(layout);
    }
    const std::vector<VertexBindingDesc> &vertexLayout() const { return _vertexLayout; }

    // Blend mode
    void setBlendMode(BlendMode mode) { _blendMode = mode; }
    BlendMode blendMode() const { return _blendMode; }

    // Depth settings
    void setDepthWrite(bool enable) { _depthWrite = enable; }
    bool depthWrite() const { return _depthWrite; }
    void setDepthTest(bool enable) { _depthTest = enable; }
    bool depthTest() const { return _depthTest; }

    // Topology
    void setTopology(Topology topo) { _topology = topo; }
    Topology topology() const { return _topology; }

    // Procedural vertex count (shaders that generate geometry)
    void setProceduralVertexCount(uint32_t count) { _proceduralVertexCount = count; }
    uint32_t proceduralVertexCount() const { return _proceduralVertexCount; }

    // Instanced rendering
    void setInstancedRendering(bool enable) { _instancedRendering = enable; }
    bool instancedRendering() const { return _instancedRendering; }

    // Voxel text layout flag
    void setUseVoxelTextLayout(bool enable) { _useVoxelTextLayout = enable; }
    bool useVoxelTextLayout() const { return _useVoxelTextLayout; }

    // Texture binding flag: shader declares uniform sampler2D at set=0, binding=0
    void setHasTextureBinding(bool enable) { _hasTextureBinding = enable; }
    bool hasTextureBinding() const { return _hasTextureBinding; }

  private:
    std::string _vertexShaderPath;
    std::string _fragmentShaderPath;
    std::vector<char> _vertexSpirv;
    std::vector<char> _fragmentSpirv;
    std::vector<VertexBindingDesc> _vertexLayout;
    BlendMode _blendMode{BlendMode::Opaque};
    bool _depthWrite{true};
    bool _depthTest{true};
    Topology _topology{Topology::TriangleList};
    uint32_t _proceduralVertexCount{0};
    bool _instancedRendering{false};
    bool _useVoxelTextLayout{false};
    bool _hasTextureBinding{false};
};

using ShaderComponentUPtr = std::unique_ptr<ShaderComponent>;
using ShaderComponentSPtr = std::shared_ptr<ShaderComponent>;

} // namespace graphics
} // namespace ecs
} // namespace vigine

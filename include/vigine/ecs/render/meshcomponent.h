#pragma once

/**
 * @file meshcomponent.h
 * @brief CPU-side mesh storage with GPU-buffer handles for upload.
 */

#include "graphicshandles.h"

#include "vigine/base/macros.h"

#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace vigine
{
namespace graphics
{

/**
 * @brief Single mesh vertex: position, colour, and texture coordinates.
 */
struct Vertex
{
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 texCoord{0.0f, 0.0f}; // UV coordinates for textures
};

/**
 * @brief CPU-side vertex / index data paired with GPU buffer handles.
 *
 * Stores a vertex list and optional index list, tracks dirty state
 * for re-upload, and carries BufferHandles for the uploaded GPU
 * buffers. Also supports procedural-in-shader meshes, where the
 * shader generates geometry and no CPU data is uploaded. Provides
 * factory helpers createCube() and createPlane().
 */
class MeshComponent
{
  public:
    MeshComponent();

    void addVertex(const glm::vec3 &position, const glm::vec3 &color);
    void addVertex(const glm::vec3 &position, const glm::vec3 &color, const glm::vec2 &texCoord);
    void addIndex(uint32_t index);

    const std::vector<Vertex> &getVertices() const { return _vertices; }
    const std::vector<uint32_t> &getIndices() const { return _indices; }

    uint32_t getVertexCount() const { return static_cast<uint32_t>(_vertices.size()); }
    uint32_t getIndexCount() const { return static_cast<uint32_t>(_indices.size()); }

    // Procedural geometry in shader (e.g., cube.vert, sphere.vert generate vertices)
    void setProceduralInShader(bool procedural, uint32_t vertexCount = 0)
    {
        _isProceduralInShader  = procedural;
        _proceduralVertexCount = vertexCount;
        if (procedural)
            _dirty = false; // Procedural meshes don't have CPU data to upload
    }
    bool isProceduralInShader() const { return _isProceduralInShader; }
    uint32_t proceduralVertexCount() const { return _proceduralVertexCount; }

    // GPU buffer handles (assigned by backend after upload)
    void setVertexBufferHandle(BufferHandle handle) { _vertexBuffer = handle; }
    BufferHandle vertexBufferHandle() const { return _vertexBuffer; }
    void setIndexBufferHandle(BufferHandle handle) { _indexBuffer = handle; }
    BufferHandle indexBufferHandle() const { return _indexBuffer; }

    // Dirty flag for re-upload tracking
    void markDirty() { _dirty = true; }
    void clearDirty() { _dirty = false; }
    bool isDirty() const { return _dirty; }

    static MeshComponent createCube();
    static MeshComponent createPlane(float width, float height, const glm::vec3 &color);

  private:
    std::vector<Vertex> _vertices;
    std::vector<uint32_t> _indices;

    // Procedural geometry flags (for shaders that generate geometry)
    bool _isProceduralInShader{false};
    uint32_t _proceduralVertexCount{0};

    // GPU buffer handles
    BufferHandle _vertexBuffer{};
    BufferHandle _indexBuffer{};

    // Dirty tracking
    bool _dirty{true};
};

using MeshComponentUPtr = std::unique_ptr<MeshComponent>;
using MeshComponentSPtr = std::shared_ptr<MeshComponent>;

} // namespace graphics
} // namespace vigine

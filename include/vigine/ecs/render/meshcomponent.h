#pragma once

#include "vigine/base/macros.h"

#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace vigine
{
namespace graphics
{

struct Vertex
{
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 texCoord{0.0f, 0.0f}; // UV coordinates for textures
};

class MeshComponent
{
  public:
    MeshComponent();

    void addVertex(const glm::vec3 &position, const glm::vec3 &color);
    void addVertex(const glm::vec3 &position, const glm::vec3 &color, const glm::vec2 &texCoord);
    void addIndex(uint32_t index);

    const std::vector<Vertex> &getVertices() const { return _vertices; }
    const std::vector<uint32_t> &getIndices() const { return _indices; }

    uint32_t getVertexCount() const { return _vertices.size(); }
    uint32_t getIndexCount() const { return _indices.size(); }

    static MeshComponent createCube();
    static MeshComponent createPlane(float width, float height, const glm::vec3 &color);

  private:
    std::vector<Vertex> _vertices;
    std::vector<uint32_t> _indices;
};

BUILD_SMART_PTR(MeshComponent);

} // namespace graphics
} // namespace vigine

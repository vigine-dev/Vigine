#include "vigine/ecs/render/meshcomponent.h"

using namespace vigine::graphics;

MeshComponent::MeshComponent() {}

void MeshComponent::addVertex(const glm::vec3 &position, const glm::vec3 &color)
{
    _vertices.push_back({position, color});
}

void MeshComponent::addVertex(const glm::vec3 &position, const glm::vec3 &color,
                              const glm::vec2 &texCoord)
{
    _vertices.push_back({position, color, texCoord});
}

void MeshComponent::addIndex(uint32_t index) { _indices.push_back(index); }

MeshComponent MeshComponent::createCube()
{
    MeshComponent mesh;

    // Front face (Z+, Red)
    mesh.addVertex({-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}); // 0
    mesh.addVertex({0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f});  // 1
    mesh.addVertex({0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f});   // 2
    mesh.addVertex({-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f});  // 3

    // Back face (Z-, Green)
    mesh.addVertex({0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f});  // 4
    mesh.addVertex({-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}); // 5
    mesh.addVertex({-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f});  // 6
    mesh.addVertex({0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f});   // 7

    // Top face (Y+, Blue)
    mesh.addVertex({-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}); // 8
    mesh.addVertex({0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f});  // 9
    mesh.addVertex({0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f});   // 10
    mesh.addVertex({-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f});  // 11

    // Bottom face (Y-, Yellow)
    mesh.addVertex({-0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 0.0f});  // 12
    mesh.addVertex({0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 0.0f});   // 13
    mesh.addVertex({0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f});  // 14
    mesh.addVertex({-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}); // 15

    // Right face (X+, Cyan)
    mesh.addVertex({0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 1.0f});  // 16
    mesh.addVertex({0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}); // 17
    mesh.addVertex({0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 1.0f});  // 18
    mesh.addVertex({0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 1.0f});   // 19

    // Left face (X-, Magenta)
    mesh.addVertex({-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}); // 20
    mesh.addVertex({-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 1.0f});  // 21
    mesh.addVertex({-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 1.0f});   // 22
    mesh.addVertex({-0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 1.0f});  // 23

    // Front face
    mesh.addIndex(0);
    mesh.addIndex(1);
    mesh.addIndex(2);
    mesh.addIndex(0);
    mesh.addIndex(2);
    mesh.addIndex(3);

    // Back face
    mesh.addIndex(4);
    mesh.addIndex(5);
    mesh.addIndex(6);
    mesh.addIndex(4);
    mesh.addIndex(6);
    mesh.addIndex(7);

    // Top face
    mesh.addIndex(8);
    mesh.addIndex(9);
    mesh.addIndex(10);
    mesh.addIndex(8);
    mesh.addIndex(10);
    mesh.addIndex(11);

    // Bottom face
    mesh.addIndex(12);
    mesh.addIndex(13);
    mesh.addIndex(14);
    mesh.addIndex(12);
    mesh.addIndex(14);
    mesh.addIndex(15);

    // Right face
    mesh.addIndex(16);
    mesh.addIndex(17);
    mesh.addIndex(18);
    mesh.addIndex(16);
    mesh.addIndex(18);
    mesh.addIndex(19);

    // Left face
    mesh.addIndex(20);
    mesh.addIndex(21);
    mesh.addIndex(22);
    mesh.addIndex(20);
    mesh.addIndex(22);
    mesh.addIndex(23);

    return mesh;
}

MeshComponent MeshComponent::createPlane(float width, float height, const glm::vec3 &color)
{
    MeshComponent mesh;

    const float hw = width * 0.5f;
    const float hh = height * 0.5f;

    mesh.addVertex({-hw, -hh, 0.0f}, color); // 0 bottom-left
    mesh.addVertex({hw, -hh, 0.0f}, color);  // 1 bottom-right
    mesh.addVertex({hw, hh, 0.0f}, color);   // 2 top-right
    mesh.addVertex({-hw, hh, 0.0f}, color);  // 3 top-left

    // Single plane (CCW viewed from +Z)
    mesh.addIndex(0);
    mesh.addIndex(1);
    mesh.addIndex(2);
    mesh.addIndex(0);
    mesh.addIndex(2);
    mesh.addIndex(3);

    return mesh;
}

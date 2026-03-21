#pragma once

#include "vigine/base/macros.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <memory>

namespace vigine
{
namespace graphics
{

class TransformComponent
{
  public:
    TransformComponent();

    void setPosition(const glm::vec3 &pos);
    void setRotation(const glm::vec3 &rot);
    void setScale(const glm::vec3 &scale);

    glm::vec3 getPosition() const { return _position; }
    glm::vec3 getRotation() const { return _rotation; }
    glm::vec3 getScale() const { return _scale; }

    glm::mat4 getModelMatrix() const;

    void rotate(const glm::vec3 &axis, float angle);

  private:
    glm::vec3 _position{0.0f, 0.0f, 0.0f};
    glm::vec3 _rotation{0.0f, 0.0f, 0.0f};
    glm::vec3 _scale{1.0f, 1.0f, 1.0f};
};

BUILD_SMART_PTR(TransformComponent);

} // namespace graphics
} // namespace vigine

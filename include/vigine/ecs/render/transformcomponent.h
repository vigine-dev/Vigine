#pragma once

/**
 * @file transformcomponent.h
 * @brief Position / rotation / scale carrier with optional billboarding.
 */

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

/**
 * @brief Stores position, rotation, and scale for a renderable entity.
 *
 * Provides getters and setters for each axis, a rotate() helper that
 * applies a local-axis rotation, getModelMatrix() for the standard
 * TRS matrix, and getBillboardModelMatrix() that faces the camera
 * when the billboard flag is enabled.
 */
class TransformComponent
{
  public:
    TransformComponent();

    void setPosition(const glm::vec3 &pos);
    void setRotation(const glm::vec3 &rot);
    void setScale(const glm::vec3 &scale);
    void setBillboard(bool enabled) { _billboard = enabled; }

    glm::vec3 getPosition() const { return _position; }
    glm::vec3 getRotation() const { return _rotation; }
    glm::vec3 getScale() const { return _scale; }
    bool isBillboard() const { return _billboard; }

    glm::mat4 getModelMatrix() const;
    glm::mat4 getBillboardModelMatrix(const glm::vec3 &cameraPosition) const;

    void rotate(const glm::vec3 &axis, float angle);

  private:
    glm::vec3 _position{0.0f, 0.0f, 0.0f};
    glm::vec3 _rotation{0.0f, 0.0f, 0.0f};
    glm::vec3 _scale{1.0f, 1.0f, 1.0f};
    bool _billboard{false};
};

BUILD_SMART_PTR(TransformComponent);

} // namespace graphics
} // namespace vigine

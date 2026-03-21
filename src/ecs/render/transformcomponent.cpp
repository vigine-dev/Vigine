#include "vigine/ecs/render/transformcomponent.h"

#include <glm/gtx/euler_angles.hpp>

using namespace vigine::graphics;

TransformComponent::TransformComponent() {}

void TransformComponent::setPosition(const glm::vec3 &pos) { _position = pos; }

void TransformComponent::setRotation(const glm::vec3 &rot) { _rotation = rot; }

void TransformComponent::setScale(const glm::vec3 &scale) { _scale = scale; }

glm::mat4 TransformComponent::getModelMatrix() const
{
    glm::mat4 model = glm::mat4(1.0f);
    model           = glm::translate(model, _position);
    model           = glm::rotate(model, _rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    model           = glm::rotate(model, _rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    model           = glm::rotate(model, _rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    model           = glm::scale(model, _scale);
    return model;
}

void TransformComponent::rotate(const glm::vec3 &axis, float angle)
{
    // This is a simplified rotation; a more sophisticated approach would use quaternions
    _rotation += axis * angle;
}

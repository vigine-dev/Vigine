#include "vigine/ecs/render/transformcomponent.h"

#include <cmath>
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

glm::mat4 TransformComponent::getBillboardModelMatrix(const glm::vec3 &cameraPosition) const
{
    const glm::vec3 toCamera = cameraPosition - _position;
    if (glm::length(toCamera) < 0.001f)
        return getModelMatrix();

    const glm::vec3 forward = glm::normalize(toCamera);
    const glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 refUp =
        (std::abs(glm::dot(forward, worldUp)) > 0.999f) ? glm::vec3(0.0f, 0.0f, -1.0f) : worldUp;

    const glm::vec3 right = glm::normalize(glm::cross(refUp, forward));
    const glm::vec3 up    = glm::cross(forward, right);

    glm::mat4 model(1.0f);
    model[0] = glm::vec4(right, 0.0f);
    model[1] = glm::vec4(up, 0.0f);
    model[2] = glm::vec4(forward, 0.0f);
    model[3] = glm::vec4(_position, 1.0f);

    model    = model * glm::scale(glm::mat4(1.0f), _scale);
    return model;
}

void TransformComponent::rotate(const glm::vec3 &axis, float angle)
{
    // This is a simplified rotation; a more sophisticated approach would use quaternions
    _rotation += axis * angle;
}

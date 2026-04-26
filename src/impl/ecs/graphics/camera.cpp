#include "vigine/impl/ecs/graphics/camera.h"

#include <algorithm>
#include <cmath>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>

namespace vigine
{
namespace ecs
{
namespace graphics
{

Camera::Camera() : Camera(CameraConfig{}) {}

Camera::Camera(const CameraConfig &config) : _config(config) {}

Camera::~Camera() = default;

void Camera::beginDrag(int x, int y)
{
    _dragActive   = true;
    _lastPointerX = x;
    _lastPointerY = y;
}

void Camera::updateDrag(int x, int y)
{
    if (!_dragActive)
        return;

    const int deltaX  = x - _lastPointerX;
    const int deltaY  = y - _lastPointerY;

    _yaw             -= static_cast<float>(deltaX) * _config.mouseLookSensitivity;
    _pitch        = std::clamp(_pitch + static_cast<float>(deltaY) * _config.mouseLookSensitivity,
                               -_config.pitchLimitRad, _config.pitchLimitRad);

    _lastPointerX = x;
    _lastPointerY = y;
}

void Camera::endDrag() { _dragActive = false; }

void Camera::zoom(int delta)
{
    _position += cameraForward() *
                 ((static_cast<float>(delta) / _config.wheelStep) * _config.wheelMovePerStep);
}

void Camera::setMoveForwardActive(bool active) { _moveForwardActive = active; }

void Camera::setMoveBackwardActive(bool active) { _moveBackwardActive = active; }

void Camera::setMoveLeftActive(bool active) { _moveLeftActive = active; }

void Camera::setMoveRightActive(bool active) { _moveRightActive = active; }

void Camera::setMoveUpActive(bool active) { _moveUpActive = active; }

void Camera::setMoveDownActive(bool active) { _moveDownActive = active; }

void Camera::setSprintActive(bool active) { _sprintActive = active; }

void Camera::update(float deltaSeconds)
{
    // Clamp delta time to avoid huge jumps
    deltaSeconds = std::clamp(deltaSeconds, 0.0f, 0.1f);

    // Calculate desired movement direction
    glm::vec3 desiredDirection(0.0f);
    const glm::vec3 forwardPlanar = flatForward();
    const glm::vec3 rightPlanar =
        glm::normalize(glm::cross(forwardPlanar, glm::vec3(0.0f, 1.0f, 0.0f)));

    if (_moveForwardActive)
        desiredDirection += forwardPlanar;
    if (_moveBackwardActive)
        desiredDirection -= forwardPlanar;
    if (_moveRightActive)
        desiredDirection += rightPlanar;
    if (_moveLeftActive)
        desiredDirection -= rightPlanar;
    if (_moveUpActive)
        desiredDirection += glm::vec3(0.0f, 1.0f, 0.0f);
    if (_moveDownActive)
        desiredDirection -= glm::vec3(0.0f, 1.0f, 0.0f);

    // Calculate desired velocity
    glm::vec3 desiredVelocity(0.0f);
    if (glm::dot(desiredDirection, desiredDirection) > 0.0f)
    {
        const float sprintFactor = _sprintActive ? _config.sprintMultiplier : 1.0f;
        desiredVelocity =
            glm::normalize(desiredDirection) * (_config.moveSpeedUnitsPerSec * sprintFactor);
    }

    // Smooth velocity interpolation
    const float response  = 1.0f - std::exp(-_config.acceleration * deltaSeconds);
    _velocity            += (desiredVelocity - _velocity) * response;

    // Stop if velocity is very small
    if (glm::dot(_velocity, _velocity) < _config.stopVelocityEpsilon * _config.stopVelocityEpsilon)
        _velocity = glm::vec3(0.0f);

    // Update position
    _position += _velocity * deltaSeconds;
}

glm::mat4 Camera::viewMatrix() const
{
    const glm::vec3 forward = cameraForward();
    return glm::lookAt(_position, _position + forward, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::projectionMatrix(float aspectRatio) const
{
    glm::mat4 proj = glm::perspective(glm::radians(_config.fovDegrees), aspectRatio,
                                      _config.nearPlane, _config.farPlane);
    // Flip Y for Vulkan
    proj[1][1] *= -1.0f;
    return proj;
}

glm::mat4 Camera::viewProjectionMatrix(float aspectRatio) const
{
    return projectionMatrix(aspectRatio) * viewMatrix();
}

glm::vec3 Camera::forward() const { return cameraForward(); }

glm::vec3 Camera::right() const
{
    const glm::vec3 forward = cameraForward();
    return glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::vec3 Camera::up() const
{
    const glm::vec3 forward = cameraForward();
    const glm::vec3 right   = this->right();
    return glm::normalize(glm::cross(right, forward));
}

bool Camera::screenPointToRay(int x, int y, uint32_t viewportWidth, uint32_t viewportHeight,
                              glm::vec3 &rayOrigin, glm::vec3 &rayDirection) const
{
    if (viewportWidth == 0 || viewportHeight == 0)
        return false;

    const float width       = static_cast<float>(viewportWidth);
    const float height      = static_cast<float>(viewportHeight);
    const float pixelX      = static_cast<float>(x) + 0.5f;
    const float pixelY      = static_cast<float>(y) + 0.5f;
    const float ndcX        = (2.0f * pixelX) / width - 1.0f;
    const float ndcY        = (2.0f * pixelY) / height - 1.0f;

    const float aspect      = width / height;
    const glm::mat4 proj    = projectionMatrix(aspect);
    const glm::mat4 view    = viewMatrix();

    const glm::mat4 invProj = glm::inverse(proj);
    const glm::mat4 invView = glm::inverse(view);

    const glm::vec4 clip(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 eye = invProj * clip;
    eye           = glm::vec4(eye.x, eye.y, -1.0f, 0.0f);

    rayDirection  = glm::normalize(glm::vec3(invView * eye));
    rayOrigin     = _position;
    return true;
}

bool Camera::screenPointToRayFromNearPlane(int x, int y, uint32_t viewportWidth,
                                           uint32_t viewportHeight, glm::vec3 &rayOrigin,
                                           glm::vec3 &rayDirection) const
{
    if (viewportWidth == 0 || viewportHeight == 0)
        return false;

    const float width       = static_cast<float>(viewportWidth);
    const float height      = static_cast<float>((std::max)(viewportHeight, 1u));

    const float pixelX      = static_cast<float>(x) + 0.5f;
    const float pixelY      = static_cast<float>(y) + 0.5f;
    const float ndcX        = (2.0f * pixelX) / width - 1.0f;
    const float ndcY        = (2.0f * pixelY) / height - 1.0f;

    const float aspect      = width / height;
    const glm::mat4 proj    = projectionMatrix(aspect);
    const glm::mat4 view    = viewMatrix();

    const glm::mat4 invProj = glm::inverse(proj);
    const glm::mat4 invView = glm::inverse(view);

    const glm::vec4 clipNear(ndcX, ndcY, -1.0f, 1.0f);
    const glm::vec4 clipFar(ndcX, ndcY, 1.0f, 1.0f);

    const glm::vec4 worldNear4 = invView * (invProj * clipNear);
    const glm::vec4 worldFar4  = invView * (invProj * clipFar);

    if (std::abs(worldNear4.w) < 1e-6f || std::abs(worldFar4.w) < 1e-6f)
        return false;

    const glm::vec3 worldNear = glm::vec3(worldNear4) / worldNear4.w;
    const glm::vec3 worldFar  = glm::vec3(worldFar4) / worldFar4.w;
    const glm::vec3 dir       = worldFar - worldNear;
    const float dirLen        = glm::length(dir);
    if (dirLen < 1e-6f)
        return false;

    rayOrigin    = worldNear;
    rayDirection = dir / dirLen;
    return true;
}

glm::vec3 Camera::cameraForward() const
{
    return glm::normalize(glm::vec3(std::cos(_pitch) * std::sin(_yaw), std::sin(_pitch),
                                    -std::cos(_pitch) * std::cos(_yaw)));
}

glm::vec3 Camera::flatForward() const
{
    return glm::normalize(glm::vec3(std::sin(_yaw), 0.0f, -std::cos(_yaw)));
}

} // namespace graphics
} // namespace ecs
} // namespace vigine

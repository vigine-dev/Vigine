#include "vigine/ecs/render/camera.h"

#include <algorithm>
#include <cmath>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

namespace vigine
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

    const int deltaX = x - _lastPointerX;
    const int deltaY = y - _lastPointerY;

    if (_mode == CameraMode::Orbit)
    {
        _yaw   -= static_cast<float>(deltaX) * _config.mouseLookSensitivity;
        _pitch  = std::clamp(_pitch + static_cast<float>(deltaY) * _config.mouseLookSensitivity,
                             -_config.pitchLimitRad, _config.pitchLimitRad);

        const float dist = glm::length(_position - _orbitTarget);
        _position        = _orbitTarget - glm::vec3(dist * std::cos(_pitch) * std::sin(_yaw),
                                                    dist * std::sin(_pitch),
                                                    -dist * std::cos(_pitch) * std::cos(_yaw));
    } else
    {
        _yaw   -= static_cast<float>(deltaX) * _config.mouseLookSensitivity;
        _pitch  = std::clamp(_pitch + static_cast<float>(deltaY) * _config.mouseLookSensitivity,
                             -_config.pitchLimitRad, _config.pitchLimitRad);
    }

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

void Camera::setRotateYawLeftActive(bool active) { _rotateYawLeftActive = active; }
void Camera::setRotateYawRightActive(bool active) { _rotateYawRightActive = active; }
void Camera::setRotatePitchUpActive(bool active) { _rotatePitchUpActive = active; }
void Camera::setRotatePitchDownActive(bool active) { _rotatePitchDownActive = active; }

void Camera::setPanLeftActive(bool active) { _panLeftActive = active; }
void Camera::setPanRightActive(bool active) { _panRightActive = active; }
void Camera::setPanUpActive(bool active) { _panUpActive = active; }
void Camera::setPanDownActive(bool active) { _panDownActive = active; }

void Camera::setZoomInActive(bool active) { _zoomInActive = active; }
void Camera::setZoomOutActive(bool active) { _zoomOutActive = active; }

void Camera::setSpeedModifier(SpeedModifier mod) { _speedModifier = mod; }
SpeedModifier Camera::speedModifier() const { return _speedModifier; }

void Camera::setCameraMode(CameraMode mode)
{
    if (mode == CameraMode::Orbit && _mode != CameraMode::Orbit)
    {
        // Place orbit target along the current look direction so the view doesn't jump.
        float dist = glm::length(_position - _orbitTarget);
        if (dist < 0.5f)
            dist = 10.0f;
        _orbitTarget = _position + cameraForward() * dist;
    }
    _mode = mode;
}
CameraMode Camera::cameraMode() const { return _mode; }

void Camera::setOrbitTarget(const glm::vec3 &t) { _orbitTarget = t; }
const glm::vec3 &Camera::orbitTarget() const { return _orbitTarget; }

void Camera::pan(float deltaX, float deltaY)
{
    const glm::vec3 offset  = right() * deltaX + up() * deltaY;
    _position              += offset;
    _orbitTarget           += offset;
}

void Camera::rotateYawStep(float angleDeg)
{
    const float rad  = glm::radians(angleDeg);
    _yaw            += rad;

    if (_mode == CameraMode::Orbit)
    {
        const float dist = glm::length(_position - _orbitTarget);
        _position        = _orbitTarget - glm::vec3(dist * std::cos(_pitch) * std::sin(_yaw),
                                                    dist * std::sin(_pitch),
                                                    -dist * std::cos(_pitch) * std::cos(_yaw));
    }
}

void Camera::rotatePitchStep(float angleDeg)
{
    const float rad = glm::radians(angleDeg);
    _pitch          = std::clamp(_pitch + rad, -_config.pitchLimitRad, _config.pitchLimitRad);

    if (_mode == CameraMode::Orbit)
    {
        const float dist = glm::length(_position - _orbitTarget);
        _position        = _orbitTarget - glm::vec3(dist * std::cos(_pitch) * std::sin(_yaw),
                                                    dist * std::sin(_pitch),
                                                    -dist * std::cos(_pitch) * std::cos(_yaw));
    }
}

void Camera::resetPosition()
{
    _position = glm::vec3(0.0f, 1.6f, 4.5f);
    _velocity = glm::vec3(0.0f);
}

void Camera::resetRotation()
{
    _yaw   = 0.0f;
    _pitch = -0.2f;
}

void Camera::resetView()
{
    resetPosition();
    resetRotation();
    _orbitTarget = glm::vec3(0.0f);
}

void Camera::frameTarget(const glm::vec3 &center, float radius)
{
    const float fovRad = glm::radians(_config.fovDegrees);
    float dist         = radius / std::tan(fovRad * 0.5f);
    dist               = std::max(dist, _config.nearPlane * 2.0f);

    _orbitTarget       = center;
    _position          = center - cameraForward() * dist;
    _velocity          = glm::vec3(0.0f);
}

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
        float speedFactor = 1.0f;
        switch (_speedModifier)
        {
        case SpeedModifier::Slow:
            speedFactor = 0.3f;
            break;
        case SpeedModifier::Fast:
            speedFactor = _config.sprintMultiplier;
            break;
        default:
            break;
        }
        // Legacy _sprintActive still works as Fast
        if (_sprintActive && _speedModifier == SpeedModifier::Normal)
            speedFactor = _config.sprintMultiplier;

        desiredVelocity =
            glm::normalize(desiredDirection) * (_config.moveSpeedUnitsPerSec * speedFactor);
    }

    // Smooth velocity interpolation
    const float response  = 1.0f - std::exp(-_config.acceleration * deltaSeconds);
    _velocity            += (desiredVelocity - _velocity) * response;

    // Stop if velocity is very small
    if (glm::dot(_velocity, _velocity) < _config.stopVelocityEpsilon * _config.stopVelocityEpsilon)
        _velocity = glm::vec3(0.0f);

    // Update position
    const glm::vec3 positionDelta  = _velocity * deltaSeconds;
    _position                     += positionDelta;
    if (_mode == CameraMode::Orbit)
        _orbitTarget += positionDelta;

    // Continuous rotation (Numpad-style, frame-rate based)
    if (_rotateYawLeftActive || _rotateYawRightActive || _rotatePitchUpActive ||
        _rotatePitchDownActive)
    {
        const float rotDelta = glm::radians(_config.rotateSpeedDegreesPerSec) * deltaSeconds;
        if (_rotateYawLeftActive)
            rotateYawStep(-glm::degrees(rotDelta));
        if (_rotateYawRightActive)
            rotateYawStep(+glm::degrees(rotDelta));
        if (_rotatePitchUpActive)
            rotatePitchStep(-glm::degrees(rotDelta));
        if (_rotatePitchDownActive)
            rotatePitchStep(+glm::degrees(rotDelta));
    }

    // Continuous pan (Shift+Numpad-style, frame-rate based)
    if (_panLeftActive || _panRightActive || _panUpActive || _panDownActive)
    {
        const float panDelta = _config.panSpeedUnitsPerSec * deltaSeconds;
        if (_panLeftActive)
            pan(-panDelta, 0.0f);
        if (_panRightActive)
            pan(+panDelta, 0.0f);
        if (_panUpActive)
            pan(0.0f, +panDelta);
        if (_panDownActive)
            pan(0.0f, -panDelta);
    }

    if (_zoomInActive || _zoomOutActive)
    {
        const float zoomDelta = _config.zoomSpeedUnitsPerSec * deltaSeconds;
        if (_zoomInActive)
            _position += cameraForward() * zoomDelta;
        if (_zoomOutActive)
            _position -= cameraForward() * zoomDelta;
    }
}

glm::mat4 Camera::viewMatrix() const
{
    if (_mode == CameraMode::Orbit)
        return glm::lookAt(_position, _orbitTarget, glm::vec3(0.0f, 1.0f, 0.0f));
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
} // namespace vigine

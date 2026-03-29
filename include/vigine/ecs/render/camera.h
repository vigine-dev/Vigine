#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace vigine
{
namespace graphics
{

/**
 * @brief Camera configuration parameters
 */
struct CameraConfig
{
    float mouseLookSensitivity{0.01f};
    float pitchLimitRad{1.45f};
    float moveSpeedUnitsPerSec{3.5f};
    float sprintMultiplier{2.1f};
    float wheelStep{120.0f};
    float wheelMovePerStep{0.8f};
    float acceleration{12.0f};
    float stopVelocityEpsilon{0.01f};
    float nearPlane{0.1f};
    float farPlane{250.0f};
    float fovDegrees{60.0f};
};

/**
 * @brief First-person camera with mouse look and WASD movement
 */
class Camera
{
  public:
    Camera();
    explicit Camera(const CameraConfig &config);
    ~Camera();

    // Input handling
    void beginDrag(int x, int y);
    void updateDrag(int x, int y);
    void endDrag();
    void zoom(int delta);

    // Movement control
    void setMoveForwardActive(bool active);
    void setMoveBackwardActive(bool active);
    void setMoveLeftActive(bool active);
    void setMoveRightActive(bool active);
    void setMoveUpActive(bool active);
    void setMoveDownActive(bool active);
    void setSprintActive(bool active);

    // Update camera physics (call every frame with delta time)
    void update(float deltaSeconds);

    // Matrix getters
    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix(float aspectRatio) const;
    glm::mat4 viewProjectionMatrix(float aspectRatio) const;

    // Direction vectors
    glm::vec3 forward() const;
    glm::vec3 right() const;
    glm::vec3 up() const;

    // Ray casting - requires viewport dimensions
    bool screenPointToRay(int x, int y, uint32_t viewportWidth, uint32_t viewportHeight,
                          glm::vec3 &rayOrigin, glm::vec3 &rayDirection) const;
    bool screenPointToRayFromNearPlane(int x, int y, uint32_t viewportWidth,
                                       uint32_t viewportHeight, glm::vec3 &rayOrigin,
                                       glm::vec3 &rayDirection) const;

    // Position/orientation access
    const glm::vec3 &position() const { return _position; }
    void setPosition(const glm::vec3 &pos) { _position = pos; }

    float yaw() const { return _yaw; }
    float pitch() const { return _pitch; }
    void setYaw(float yaw) { _yaw = yaw; }
    void setPitch(float pitch) { _pitch = pitch; }

    const CameraConfig &config() const { return _config; }
    void setConfig(const CameraConfig &config) { _config = config; }

  private:
    CameraConfig _config;

    // Camera state
    float _yaw{0.0f};
    float _pitch{-0.2f};
    glm::vec3 _position{0.0f, 1.6f, 4.5f};
    glm::vec3 _velocity{0.0f, 0.0f, 0.0f};

    // Drag state
    bool _dragActive{false};
    int _lastPointerX{0};
    int _lastPointerY{0};

    // Movement flags
    bool _moveForwardActive{false};
    bool _moveBackwardActive{false};
    bool _moveLeftActive{false};
    bool _moveRightActive{false};
    bool _moveUpActive{false};
    bool _moveDownActive{false};
    bool _sprintActive{false};

    // Helper functions
    glm::vec3 cameraForward() const;
    glm::vec3 flatForward() const;
};

} // namespace graphics
} // namespace vigine

#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace vigine
{
namespace graphics
{

enum class CameraMode
{
    FreeLook,
    Orbit
};

enum class SpeedModifier
{
    Normal,
    Slow,
    Fast
};

/**
 * @brief Camera configuration parameters
 */
struct CameraConfig
{
    float mouseLookSensitivity{0.01f};
    float pitchLimitRad{1.45f};
    float moveSpeedUnitsPerSec{20.0f};
    float sprintMultiplier{2.1f};
    float wheelStep{120.0f};
    float wheelMovePerStep{0.8f};
    float acceleration{12.0f};
    float stopVelocityEpsilon{0.01f};
    float nearPlane{0.1f};
    float farPlane{250.0f};
    float fovDegrees{60.0f};
    float rotateSpeedDegreesPerSec{90.0f};
    float panSpeedUnitsPerSec{3.5f};
    float zoomSpeedUnitsPerSec{5.0f};
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

    // Continuous rotation control (per-frame, like WASD movement)
    void setRotateYawLeftActive(bool active);
    void setRotateYawRightActive(bool active);
    void setRotatePitchUpActive(bool active);
    void setRotatePitchDownActive(bool active);

    // Continuous pan control (per-frame)
    void setPanLeftActive(bool active);
    void setPanRightActive(bool active);
    void setPanUpActive(bool active);
    void setPanDownActive(bool active);

    // Continuous zoom control (per-frame)
    void setZoomInActive(bool active);
    void setZoomOutActive(bool active);

    // Speed modifier (Slow=0.3x, Normal=1x, Fast=sprintMultiplier)
    void setSpeedModifier(SpeedModifier mod);
    SpeedModifier speedModifier() const;

    // Camera mode
    void setCameraMode(CameraMode mode);
    CameraMode cameraMode() const;

    // Orbit target (used in Orbit mode)
    void setOrbitTarget(const glm::vec3 &target);
    const glm::vec3 &orbitTarget() const;

    // Pan: shift camera and orbit target by camera-right / camera-up delta
    void pan(float deltaX, float deltaY);

    // Discrete rotation steps (degrees); respects CameraMode for orbit vs free
    void rotateYawStep(float angleDeg);
    void rotatePitchStep(float angleDeg);

    // Reset helpers
    void resetPosition();
    void resetRotation();
    void resetView();

    // Frame: position camera so that a sphere of (center, radius) fits the view
    void frameTarget(const glm::vec3 &center, float radius);

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

    // Continuous rotation flags (Numpad-style, per-frame)
    bool _rotateYawLeftActive{false};
    bool _rotateYawRightActive{false};
    bool _rotatePitchUpActive{false};
    bool _rotatePitchDownActive{false};

    // Continuous pan flags (Shift+Numpad-style, per-frame)
    bool _panLeftActive{false};
    bool _panRightActive{false};
    bool _panUpActive{false};
    bool _panDownActive{false};

    // Continuous zoom flags (Numpad +/-, per-frame)
    bool _zoomInActive{false};
    bool _zoomOutActive{false};

    CameraMode _mode{CameraMode::FreeLook};
    glm::vec3 _orbitTarget{0.0f, 0.0f, 0.0f};
    SpeedModifier _speedModifier{SpeedModifier::Normal};

    // Helper functions
    glm::vec3 cameraForward() const;
    glm::vec3 flatForward() const;
};

} // namespace graphics
} // namespace vigine

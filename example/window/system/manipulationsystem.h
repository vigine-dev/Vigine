#pragma once

#include <glm/vec3.hpp>
#include <vector>

namespace vigine
{
class Entity;
class EntityManager;
namespace graphics
{
class GraphicsService;
class RenderSystem;
} // namespace graphics
} // namespace vigine

enum class TransformMode
{
    None,
    Grab
};
enum class AxisConstraint
{
    None,
    X,
    Y,
    Z
};
enum class CoordSystem
{
    Global,
    Local
};

struct ManipulationState
{
    TransformMode mode{TransformMode::None};
    AxisConstraint constraint{AxisConstraint::None};
    CoordSystem coordSystem{CoordSystem::Global};

    glm::vec3 initialPosition{0.0f};
    glm::vec3 initialRotation{0.0f};
    glm::vec3 initialScale{1.0f};

    int startMouseX{0};
    int startMouseY{0};
    int lastMouseX{0};
    int lastMouseY{0};
};

class ManipulationSystem
{
  public:
    ManipulationSystem(vigine::graphics::GraphicsService *graphicsService,
                       vigine::graphics::RenderSystem *renderSystem,
                       vigine::EntityManager *entityManager);

    bool isActive() const;
    TransformMode mode() const;
    CoordSystem coordSystem() const;

    void beginGrab(vigine::Entity *entity, int mouseX, int mouseY);

    void setAxisConstraint(AxisConstraint constraint);

    void updateFromMouse(int mouseX, int mouseY);

    // Confirm: keep current transform, deactivate mode.
    void confirm();

    // Cancel: restore initial transform, deactivate mode.
    void cancel();

    // Duplicate selected entities and enter Grab mode for new ones.
    // Returns the vector of newly created entities.
    std::vector<vigine::Entity *> duplicateEntities(const std::vector<vigine::Entity *> &entities,
                                                    int mouseX, int mouseY);

    // Delete entities (skips protected ones like MainWindow).
    void deleteEntities(const std::vector<vigine::Entity *> &entities);

  private:
    void applyTransform(const glm::vec3 &position, const glm::vec3 &rotation,
                        const glm::vec3 &scale);
    glm::vec3 applyAxisConstraint(const glm::vec3 &delta) const;

    vigine::graphics::GraphicsService *_graphicsService{nullptr};
    vigine::graphics::RenderSystem *_renderSystem{nullptr};
    vigine::EntityManager *_entityManager{nullptr};

    vigine::Entity *_targetEntity{nullptr};
    ManipulationState _state;
};

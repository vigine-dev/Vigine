#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace vigine
{
namespace graphics
{

// Shared push constant layout used by all entity and SDF pipelines.
// Total size must not exceed the Vulkan minPushConstantsSize guarantee (128 bytes).
struct PushConstants
{
    glm::mat4 viewProjection{1.0f};
    glm::vec4 animationData{0.0f};
    glm::vec4 sunDirectionIntensity{0.0f};
    glm::vec4 lightingParams{0.0f};
    glm::mat4 modelMatrix{1.0f};
};

} // namespace graphics
} // namespace vigine

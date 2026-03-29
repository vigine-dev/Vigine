#pragma once

#include <glm/vec4.hpp>

namespace vigine
{
namespace graphics
{

struct SelectionComponent
{
    bool selected{false};
    bool hovered{false};
    glm::vec4 selectionColor{1.0f, 0.5f, 0.0f, 1.0f}; // orange tint
    glm::vec4 hoverColor{0.8f, 0.8f, 1.0f, 0.5f};     // light blue tint
};

} // namespace graphics
} // namespace vigine

#include "vigine/ecs/render/rendercomponent.h"

#include <iostream>

namespace vigine
{

RenderComponent::RenderComponent()
{
    // Конструктор
}

void RenderComponent::render() { std::cout << "Rendering component" << std::endl; }

} // namespace vigine

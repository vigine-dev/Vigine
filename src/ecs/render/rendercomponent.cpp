#include "vigine/ecs/render/rendercomponent.h"

#include <iostream>

using namespace vigine::graphics;

RenderComponent::RenderComponent() {}

RenderComponent::~RenderComponent() {};

void RenderComponent::render() { std::cout << "Rendering component" << std::endl; }

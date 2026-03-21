#include "vigine/ecs/render/rendercomponent.h"

#include <iostream>

using namespace vigine::graphics;

RenderComponent::RenderComponent() {}

RenderComponent::~RenderComponent() {};

void RenderComponent::render() {}

void RenderComponent::setMesh(const MeshComponent &mesh) { _mesh = mesh; }

void RenderComponent::setTransform(const TransformComponent &transform) { _transform = transform; }

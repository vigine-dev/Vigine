#include "vigine/ecs/render/rendersystem.h"

#include "vigine/ecs/render/rendercomponent.h"
#include "vigine/ecs/render/vulkanapi.h"

#include <iostream>

using namespace vigine::graphics;

RenderSystem::RenderSystem(const SystemName &name) : AbstractSystem(name) {}

RenderSystem::~RenderSystem() {}

bool RenderSystem::hasComponents(Entity *entity) const { return false; }

void RenderSystem::createComponents(Entity *entity) {}

void RenderSystem::destroyComponents(Entity *entity) {}

vigine::SystemId RenderSystem::id() const { return "Render"; }

void RenderSystem::update() { std::cout << "Updating render system" << std::endl; }

void RenderSystem::entityBound() {}

void RenderSystem::entityUnbound() {}

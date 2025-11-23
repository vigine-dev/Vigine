#include "vigine/ecs/platform/windowsystem.h"

using namespace vigine::platform;

WindowSystem::WindowSystem(const SystemName &name) : AbstractSystem(name) {}

WindowSystem::~WindowSystem() = default;

vigine::SystemId WindowSystem::id() const {}

bool WindowSystem::hasComponents(Entity *entity) const { return false; }

void WindowSystem::createComponents(Entity *entity) {}

void WindowSystem::destroyComponents(Entity *entity) {}

void WindowSystem::entityBound() {}

void WindowSystem::entityUnbound() {}

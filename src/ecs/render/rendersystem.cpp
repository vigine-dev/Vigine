#include "vigine/ecs/render/rendersystem.h"

#include "vigine/ecs/render/rendercomponent.h"
#include "vigine/ecs/render/vulkanapi.h"

#include <iostream>

using namespace vigine::graphics;

// COPILOT_TODO: Ініціалізувати _boundEntityComponent та _vulkanAPI в initializer list; зараз
// життєвий цикл системи фактично не визначений.
RenderSystem::RenderSystem(const SystemName &name) : AbstractSystem(name) {}

RenderSystem::~RenderSystem() {}

// COPILOT_TODO: Реалізувати реальну перевірку наявності компонентів; постійний false ламає
// create/bind логіку сервісу поверх цієї системи.
bool RenderSystem::hasComponents(Entity *entity) const { return false; }

// COPILOT_TODO: Створювати та реєструвати RenderComponent для entity, інакше система ніколи не
// переходить із стану заглушки в робочий режим.
void RenderSystem::createComponents(Entity *entity) {}

void RenderSystem::destroyComponents(Entity *entity) {}

vigine::SystemId RenderSystem::id() const { return "Render"; }

// COPILOT_TODO: Замінити stdout-заглушку на реальний render/update pipeline або прибрати метод із
// публічного контракту до готовності.
void RenderSystem::update() { std::cout << "Updating render system" << std::endl; }

void RenderSystem::entityBound() {}

void RenderSystem::entityUnbound() {}

#include "vigine/service/graphicsservice.h"

#include "vigine/ecs/render/rendersystem.h"

vigine::graphics::GraphicsService::GraphicsService(const Name &name) : AbstractService(name) {}

// COPILOT_TODO: Отримувати або створювати RenderSystem через Context, інакше GraphicsService
// лишається нефункціональною оболонкою.
void vigine::graphics::GraphicsService::contextChanged() {}

// COPILOT_TODO: Прив'язка сутності має створювати/render-компоненти або явно делегувати це системі;
// зараз bound entity ніяк не використовується.
void vigine::graphics::GraphicsService::entityBound()
{
    // Entity *ent = getBoundEntity();
}

void vigine::graphics::GraphicsService::entityUnbound() {}

vigine::ServiceId vigine::graphics::GraphicsService::id() const { return "Graphics"; }

#include "vigine/service/graphicsservice.h"

#include "vigine/ecs/render/rendersystem.h"

vigine::graphics::GraphicsService::GraphicsService(const Name &name) : AbstractService(name) {}

void vigine::graphics::GraphicsService::contextChanged() {}

void vigine::graphics::GraphicsService::entityBound()
{
    // Entity *ent = getBoundEntity();
}

void vigine::graphics::GraphicsService::entityUnbound() {}

vigine::ServiceId vigine::graphics::GraphicsService::id() const { return "Graphics"; }

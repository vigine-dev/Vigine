#include "texteditorservice.h"

#include <vigine/api/context/icontext.h>
#include <vigine/api/ecs/ientitymanager.h>
#include <vigine/api/service/iservice.h>
#include <vigine/api/service/wellknown.h>
#include <vigine/impl/ecs/entitymanager.h>
#include <vigine/impl/ecs/graphics/graphicsservice.h>
#include <vigine/impl/ecs/graphics/rendersystem.h>

#include "../system/texteditorsystem.h"
#include "../texteditstate.h"

TextEditorService::TextEditorService()
    : vigine::service::AbstractService()
    , _state{std::make_shared<TextEditState>()}
    , _system{std::make_shared<TextEditorSystem>(_state)}
{
}

TextEditorService::~TextEditorService() = default;

void TextEditorService::ensureWired(vigine::IContext &context)
{
    if (_wired)
        return;

    auto *entityManager = dynamic_cast<vigine::EntityManager *>(&context.entityManager());
    if (entityManager == nullptr)
        return;

    auto graphicsService = context.service(vigine::service::wellknown::graphicsService);
    if (!graphicsService)
        return;

    auto *graphics =
        dynamic_cast<vigine::ecs::graphics::GraphicsService *>(graphicsService.get());
    if (graphics == nullptr)
        return;

    auto *renderSystem = graphics->renderSystem();
    if (renderSystem == nullptr)
        return;

    _system->bind(entityManager, graphics, renderSystem);
    _wired = true;
}

void TextEditorService::bindInteractionEntity(vigine::Entity *entity)
{
    static_cast<void>(_system->bindInteractionEntity(entity));
}

std::shared_ptr<TextEditState> TextEditorService::state() const noexcept
{
    return _state;
}

std::shared_ptr<TextEditorSystem> TextEditorService::textEditorSystem() const noexcept
{
    return _system;
}

// ---- Window-event router (forwards to TextEditorSystem::route*) ----------

void TextEditorService::onMouseButtonDown(vigine::ecs::platform::MouseButton button, int x, int y)
{
    _system->routeMouseButtonDown(button, x, y);
}

void TextEditorService::onMouseButtonUp(vigine::ecs::platform::MouseButton button, int x, int y)
{
    _system->routeMouseButtonUp(button, x, y);
}

void TextEditorService::onMouseMove(int x, int y)
{
    _system->routeMouseMove(x, y);
}

void TextEditorService::onMouseWheel(int delta, int x, int y)
{
    _system->routeMouseWheel(delta, x, y);
}

void TextEditorService::onKeyDown(const vigine::ecs::platform::KeyEvent &event)
{
    _system->routeKeyDown(event);
}

void TextEditorService::onKeyUp(const vigine::ecs::platform::KeyEvent &event)
{
    _system->routeKeyUp(event);
}

void TextEditorService::onChar(const vigine::ecs::platform::TextEvent &event)
{
    _system->routeChar(event);
}

void TextEditorService::onWindowResized(int width, int height)
{
    _system->routeWindowResized(width, height);
}

void TextEditorService::onFrame()
{
    _system->onFrame();
}

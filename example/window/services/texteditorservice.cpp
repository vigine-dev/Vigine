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

    // EntityManager comes from the engine-default slot; the concrete
    // is the legacy @c vigine::EntityManager so the dynamic_cast is
    // expected to succeed for the default path. A future override
    // installed via @c IContext::setEntityManager that supplies a
    // non-EntityManager implementation will fail this cast and leave
    // the wired flag clear; the service's own getters still hand back
    // live handles so the example can still read state, but the
    // @c TextEditorSystem stays unbound until the override matches
    // the legacy concrete.
    auto *entityManager = dynamic_cast<vigine::EntityManager *>(&context.entityManager());
    if (entityManager == nullptr)
        return;

    auto graphicsService =
        context.service(vigine::service::wellknown::graphicsService);
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

std::shared_ptr<TextEditState> TextEditorService::state() const noexcept
{
    return _state;
}

std::shared_ptr<TextEditorSystem> TextEditorService::textEditorSystem() const noexcept
{
    return _system;
}

#include "uiservice.h"
#include "../system/uisystem.h"

#include <vigine/context.h>
#include <vigine/property.h>
#include <vigine/service/graphicsservice.h>

UIService::UIService(const vigine::Name& name) : vigine::AbstractService(name) {}

UIService::~UIService() = default;

UISystem* UIService::uiSystem() const
{
    return _uiSystem;
}

void UIService::contextChanged()
{
    if (!context())
    {
        delete _uiSystem;
        _uiSystem = nullptr;
        return;
    }

    if (_uiSystem)
        return;

    auto* entityManager = context()->entityManager();

    auto* graphicsService = dynamic_cast<vigine::graphics::GraphicsService*>(
        context()->service("Graphics", vigine::Name("MainGraphics"), vigine::Property::Exist));

    _uiSystem = new UISystem("MainUI", entityManager, graphicsService);
}

#include "setuptexttask.h"

#include <vigine/context.h>
#include <vigine/ecs/entitymanager.h>
#include <vigine/ecs/render/rendercomponent.h>
#include <vigine/ecs/render/textcomponent.h>
#include <vigine/ecs/render/transformcomponent.h>
#include <vigine/property.h>
#include <vigine/service/graphicsservice.h>

#include <filesystem>
#include <string>
#include <vector>

namespace
{
std::string resolveFontPath()
{
    const std::vector<std::string> candidates = {
        "assets/fonts/segoeui.ttf",
        "example/window/assets/fonts/segoeui.ttf",
        "../../example/window/assets/fonts/segoeui.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
    };

    for (const auto &candidate : candidates)
    {
        if (std::filesystem::exists(candidate))
            return candidate;
    }

    return {};
}

} // namespace

SetupTextTask::SetupTextTask() {}

void SetupTextTask::contextChanged()
{
    if (!context())
    {
        _graphicsService = nullptr;
        return;
    }

    _graphicsService = dynamic_cast<vigine::graphics::GraphicsService *>(
        context()->service("Graphics", vigine::Name("MainGraphics"), vigine::Property::Exist));

    if (!_graphicsService)
    {
        _graphicsService = dynamic_cast<vigine::graphics::GraphicsService *>(
            context()->service("Graphics", vigine::Name("MainGraphics"), vigine::Property::New));
    }
}

vigine::Result SetupTextTask::execute()
{
    if (!_graphicsService)
    {
        return vigine::Result(vigine::Result::Code::Error, "Graphics service is unavailable");
    }

    const auto fontPath = resolveFontPath();
    if (fontPath.empty())
    {
        return vigine::Result(vigine::Result::Code::Error,
                              "Font file not found (assets/fonts/segoeui.ttf)");
    }

    auto *entityManager = context()->entityManager();
    auto *textEntity    = entityManager->createEntity();
    if (!textEntity)
    {
        return vigine::Result(vigine::Result::Code::Error, "Failed to create text entity");
    }

    entityManager->addAlias(textEntity, "TextEntity");

    _graphicsService->bindEntity(textEntity);

    auto *renderComponent = _graphicsService->renderComponent();
    if (!renderComponent)
    {
        _graphicsService->unbindEntity();
        return vigine::Result(vigine::Result::Code::Error,
                              "Render component is unavailable for TextEntity");
    }

    vigine::graphics::TransformComponent transform;
    transform.setPosition({0.0f, 2.1f, 0.6f});
    transform.setScale({1.0f, 1.0f, 1.0f});
    renderComponent->setTransform(transform);
    renderComponent->setShaderProfile(vigine::graphics::RenderComponent::ShaderProfile::TextVoxel);

    vigine::graphics::TextComponent text;
    text.setEnabled(true);
    text.setDrawBaseInstance(false);
    constexpr char8_t kTextUtf8[] = u8"Привіт, vigine!";
    text.setText(std::string(reinterpret_cast<const char *>(kTextUtf8), sizeof(kTextUtf8) - 1));
    text.setFontPath(fontPath);
    text.setPixelSize(52);
    text.setVoxelSize(0.055f);

    if (!renderComponent->setText(text))
    {
        _graphicsService->unbindEntity();
        return vigine::Result(vigine::Result::Code::Error,
                              "Failed to build voxel text via FreeType");
    }

    _graphicsService->unbindEntity();

    return vigine::Result();
}

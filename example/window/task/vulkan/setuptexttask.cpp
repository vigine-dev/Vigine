#include "setuptexttask.h"

#include <vigine/context.h>
#include <vigine/impl/ecs/entitymanager.h>
#include <vigine/impl/ecs/graphics/rendercomponent.h>
#include <vigine/impl/ecs/graphics/shadercomponent.h>
#include <vigine/impl/ecs/graphics/textcomponent.h>
#include <vigine/impl/ecs/graphics/transformcomponent.h>
#include <vigine/property.h>
#include <vigine/impl/ecs/graphics/graphicsservice.h>

#include <filesystem>
#include <glm/glm.hpp>
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

    _graphicsService = dynamic_cast<vigine::ecs::graphics::GraphicsService *>(
        context()->service("Graphics", vigine::Name("MainGraphics"), vigine::Property::Exist));

    if (!_graphicsService)
    {
        _graphicsService = dynamic_cast<vigine::ecs::graphics::GraphicsService *>(
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

    vigine::ecs::graphics::TransformComponent transform;
    transform.setPosition({0.0f, 2.1f, 0.6f});
    transform.setScale({1.0f, 1.0f, 1.0f});
    renderComponent->setTransform(transform);

    // Configure mesh for voxel text instanced rendering
    vigine::ecs::graphics::MeshComponent voxelMesh;
    voxelMesh.setProceduralInShader(true, 36); // 36 vertices per voxel cube instance
    renderComponent->setMesh(voxelMesh);

    {
        vigine::ecs::graphics::ShaderComponent shader("textvoxel.vert.spv", "textvoxel.frag.spv");
        // Voxel text shader generates cube per character instance (36 vertices)
        shader.setUseVoxelTextLayout(true);
        shader.setInstancedRendering(true);
        // Instance vertex layout: mat4 as 4 consecutive vec4 attributes at binding 0.
        vigine::ecs::graphics::VertexBindingDesc instBinding;
        instBinding.binding      = 0;
        instBinding.stride       = sizeof(glm::mat4);
        instBinding.instanceRate = true;
        instBinding.attributes   = {
            {0, vigine::ecs::graphics::VertexFormat::Float32x4, 0 },
            {1, vigine::ecs::graphics::VertexFormat::Float32x4, 16},
            {2, vigine::ecs::graphics::VertexFormat::Float32x4, 32},
            {3, vigine::ecs::graphics::VertexFormat::Float32x4, 48},
        };
        shader.setVertexLayout({instBinding});
        renderComponent->setShader(shader);
    }

    vigine::ecs::graphics::TextComponent text;
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

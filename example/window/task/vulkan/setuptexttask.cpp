#include "setuptexttask.h"

#include <vigine/api/engine/iengine_token.h>
#include <vigine/impl/ecs/entitymanager.h>
#include <vigine/impl/ecs/graphics/rendercomponent.h>
#include <vigine/impl/ecs/graphics/rendersystem.h>
#include <vigine/impl/ecs/graphics/shadercomponent.h>
#include <vigine/impl/ecs/graphics/textcomponent.h>
#include <vigine/impl/ecs/graphics/transformcomponent.h>
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

SetupTextTask::SetupTextTask() = default;

void SetupTextTask::setEntityManager(vigine::EntityManager *entityManager) noexcept
{
    _entityManager = entityManager;
}

void SetupTextTask::setGraphicsServiceId(vigine::service::ServiceId id) noexcept
{
    _graphicsServiceId = id;
}

vigine::Result SetupTextTask::run()
{
    if (!_entityManager)
        return vigine::Result(vigine::Result::Code::Error, "EntityManager is unavailable");

    auto *token = apiToken();
    if (!token)
        return vigine::Result(vigine::Result::Code::Error, "Engine token is unavailable");

    auto graphicsResult = token->service(_graphicsServiceId);
    if (!graphicsResult.ok())
        return vigine::Result(vigine::Result::Code::Error, "Graphics service is unavailable");

    auto *graphicsService =
        dynamic_cast<vigine::ecs::graphics::GraphicsService *>(&graphicsResult.value());
    if (!graphicsService || !graphicsService->renderSystem())
        return vigine::Result(vigine::Result::Code::Error, "Graphics service is unavailable");

    auto *renderSystem = graphicsService->renderSystem();

    const auto fontPath = resolveFontPath();
    if (fontPath.empty())
        return vigine::Result(vigine::Result::Code::Error,
                              "Font file not found (assets/fonts/segoeui.ttf)");

    auto *textEntity = _entityManager->createEntity();
    if (!textEntity)
        return vigine::Result(vigine::Result::Code::Error, "Failed to create text entity");

    _entityManager->addAlias(textEntity, "TextEntity");

    renderSystem->createComponents(textEntity);
    renderSystem->bindEntity(textEntity);

    auto *renderComponent = graphicsService->renderComponent();
    if (!renderComponent)
    {
        renderSystem->unbindEntity();
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
        renderSystem->unbindEntity();
        return vigine::Result(vigine::Result::Code::Error,
                              "Failed to build voxel text via FreeType");
    }

    renderSystem->unbindEntity();

    return vigine::Result();
}

#include "setuptextedittask.h"

#include <vigine/api/engine/iengine_token.h>
#include <vigine/impl/ecs/entitymanager.h>
#include <vigine/impl/ecs/graphics/meshcomponent.h>
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
constexpr float kPanelWidth           = 4.8f;
constexpr float kPanelHeight          = 1.5f;
constexpr float kPanelCenterY         = 1.65f;
constexpr float kPanelZ               = 1.2f;
constexpr float kScrollbarWidth       = 0.06f;
constexpr float kScrollbarHeight      = 1.38f;
constexpr float kScrollbarThumbHeight = 0.26f;
constexpr float kScrollbarX           = 2.33f;
constexpr float kFocusFrameZ          = 1.209f;

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

SetupTextEditTask::SetupTextEditTask(std::shared_ptr<TextEditState> state,
                                     std::shared_ptr<TextEditorSystem> editorSystem)
    : _state(std::move(state)), _editorSystem(std::move(editorSystem))
{
}

void SetupTextEditTask::setEntityManager(vigine::EntityManager *entityManager) noexcept
{
    _entityManager = entityManager;
}

void SetupTextEditTask::setGraphicsServiceId(vigine::service::ServiceId id) noexcept
{
    _graphicsServiceId = id;
}

vigine::Result SetupTextEditTask::run()
{
    if (!_entityManager)
        return vigine::Result(vigine::Result::Code::Error, "EntityManager is unavailable");

    auto *token = api();
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

    if (_editorSystem)
        _editorSystem->bind(_entityManager, graphicsService, renderSystem);

    if (!_state)
        return vigine::Result(vigine::Result::Code::Error, "TextEditState is null");

    const auto fontPath = resolveFontPath();
    if (fontPath.empty())
        return vigine::Result(vigine::Result::Code::Error,
                              "Font file not found (assets/fonts/segoeui.ttf)");

    // --- Background panel ---
    {
        auto *bgEntity = _entityManager->createEntity();
        if (!bgEntity)
            return vigine::Result(vigine::Result::Code::Error, "Failed to create TextEditBgEntity");

        _entityManager->addAlias(bgEntity, "TextEditBgEntity");
        renderSystem->bindEntity(bgEntity);

        auto *rc = graphicsService->renderComponent();
        if (!rc)
        {
            renderSystem->unbindEntity();
            return vigine::Result(vigine::Result::Code::Error,
                                  "Render component unavailable for TextEditBgEntity");
        }

        // Flat dark-blue panel: slightly larger and moved toward camera (in front of cube).
        auto panelMesh = vigine::ecs::graphics::MeshComponent::createPlane(kPanelWidth, kPanelHeight,
                                                                      {0.08f, 0.08f, 0.25f});
        panelMesh.setProceduralInShader(true, 6); // Panel shader generates quad (6 vertices)
        rc->setMesh(panelMesh);
        {
            vigine::ecs::graphics::ShaderComponent shader("panel.vert.spv", "panel.frag.spv");
            rc->setShader(shader);
        }

        vigine::ecs::graphics::TransformComponent transform;
        transform.setPosition({0.0f, kPanelCenterY, kPanelZ});
        // Scale to actual panel dimensions so the unit-quad shader renders kPanelWidth x
        // kPanelHeight
        transform.setScale({kPanelWidth, kPanelHeight, 0.02f});
        rc->setTransform(transform);

        renderSystem->unbindEntity();
    }

    // --- Vertical scrollbar track + thumb ---
    {
        auto *trackEntity = _entityManager->createEntity();
        if (!trackEntity)
            return vigine::Result(vigine::Result::Code::Error,
                                  "Failed to create TextEditScrollbarTrackEntity");

        _entityManager->addAlias(trackEntity, "TextEditScrollbarTrackEntity");
        renderSystem->bindEntity(trackEntity);

        auto *rc = graphicsService->renderComponent();
        if (!rc)
        {
            renderSystem->unbindEntity();
            return vigine::Result(vigine::Result::Code::Error,
                                  "Render component unavailable for TextEditScrollbarTrackEntity");
        }

        auto trackMesh =
            vigine::ecs::graphics::MeshComponent::createPlane(1.0f, 1.0f, {0.18f, 0.18f, 0.32f});
        trackMesh.setProceduralInShader(true, 6);
        rc->setMesh(trackMesh);
        {
            vigine::ecs::graphics::ShaderComponent shader("panel.vert.spv", "panel.frag.spv");
            rc->setShader(shader);
        }

        vigine::ecs::graphics::TransformComponent transform;
        transform.setPosition({kScrollbarX, kPanelCenterY, kPanelZ + 0.005f});
        transform.setScale({kScrollbarWidth, kScrollbarHeight, 0.01f});
        rc->setTransform(transform);
        renderSystem->unbindEntity();

        auto *thumbEntity = _entityManager->createEntity();
        if (!thumbEntity)
            return vigine::Result(vigine::Result::Code::Error,
                                  "Failed to create TextEditScrollbarThumbEntity");

        _entityManager->addAlias(thumbEntity, "TextEditScrollbarThumbEntity");
        renderSystem->bindEntity(thumbEntity);

        rc = graphicsService->renderComponent();
        if (!rc)
        {
            renderSystem->unbindEntity();
            return vigine::Result(vigine::Result::Code::Error,
                                  "Render component unavailable for TextEditScrollbarThumbEntity");
        }

        auto thumbMesh =
            vigine::ecs::graphics::MeshComponent::createPlane(1.0f, 1.0f, {0.78f, 0.82f, 0.95f});
        thumbMesh.setProceduralInShader(true, 6);
        rc->setMesh(thumbMesh);
        {
            vigine::ecs::graphics::ShaderComponent shader("panel.vert.spv", "panel.frag.spv");
            rc->setShader(shader);
        }

        transform.setPosition({kScrollbarX, 2.14f, kPanelZ + 0.006f});
        transform.setScale({kScrollbarWidth * 0.82f, kScrollbarThumbHeight, 0.012f});
        rc->setTransform(transform);

        renderSystem->unbindEntity();
    }

    // --- Editable text (bitmap-style with individual character planes) ---
    {
        auto *textEntity = _entityManager->createEntity();
        if (!textEntity)
            return vigine::Result(vigine::Result::Code::Error, "Failed to create TextEditEntity");

        _entityManager->addAlias(textEntity, "TextEditEntity");
        renderSystem->bindEntity(textEntity);

        auto *rc = graphicsService->renderComponent();
        if (!rc)
        {
            renderSystem->unbindEntity();
            return vigine::Result(vigine::Result::Code::Error,
                                  "Render component unavailable for TextEditEntity");
        }

        vigine::ecs::graphics::TextComponent text;
        text.setEnabled(true);
        text.setDrawBaseInstance(false);
        text.setText(_state->text);
        text.setCursorBytePos(_state->cursorPos);
        text.setCursorVisible(_state->showCursor);
        text.setFontPath(fontPath);
        text.setPixelSize(28);
        text.setVoxelSize(0.0022f);
        text.setTopLeftAnchor(true);
        // Shift text from entity center to top-left of panel (panel 4.8x1.5, inset 0.05)
        text.setAnchorOffset({-2.35f, 0.7f});
        // Wrap at panel usable width (4.8 - 2*0.05 inset)
        text.setMaxLineWorldWidth(4.58f);

        // Configure mesh for glyph instanced rendering
        vigine::ecs::graphics::MeshComponent glyphMesh;
        glyphMesh.setProceduralInShader(true, 6); // 6 vertices per glyph quad instance
        rc->setMesh(glyphMesh);

        // Set shader before setText so that RenderComponent knows to build SDF quads.
        {
            vigine::ecs::graphics::ShaderComponent shader("glyph.vert.spv", "glyph.frag.spv");
            // Glyph shader generates procedural quad per glyph instance (6 vertices)
            shader.setInstancedRendering(true);
            // Instance vertex layout: mat4 as 4 consecutive vec4 attributes.
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
            rc->setShader(shader);
        }

        if (!rc->setText(text))
        {
            renderSystem->unbindEntity();
            return vigine::Result(vigine::Result::Code::Error,
                                  "Failed to build editor text voxels");
        }

        vigine::ecs::graphics::TransformComponent transform;
        // Entity stays at panel center (proven visible); anchorOffset shifts text to top-left.
        transform.setPosition({0.0f, kPanelCenterY, 1.21f});
        transform.setScale({1.0f, 1.0f, 1.0f});
        rc->setTransform(transform);

        renderSystem->unbindEntity();
    }

    // --- Focus frame (4 thin lines), hidden until editor gets focus ---
    {
        const std::vector<std::string> aliases = {
            "TextEditFocusTopEntity",
            "TextEditFocusBottomEntity",
            "TextEditFocusLeftEntity",
            "TextEditFocusRightEntity",
        };

        for (const auto &alias : aliases)
        {
            auto *e = _entityManager->createEntity();
            if (!e)
                return vigine::Result(vigine::Result::Code::Error,
                                      "Failed to create focus frame entity");

            _entityManager->addAlias(e, alias);
            renderSystem->bindEntity(e);

            auto *rc = graphicsService->renderComponent();
            if (!rc)
            {
                renderSystem->unbindEntity();
                return vigine::Result(vigine::Result::Code::Error,
                                      "Render component unavailable for focus frame entity");
            }

            auto frameMesh =
                vigine::ecs::graphics::MeshComponent::createPlane(1.0f, 1.0f, {1.0f, 1.0f, 1.0f});
            frameMesh.setProceduralInShader(true, 6);
            rc->setMesh(frameMesh);
            {
                vigine::ecs::graphics::ShaderComponent shader("panel.vert.spv", "panel.frag.spv");
                rc->setShader(shader);
            }

            vigine::ecs::graphics::TransformComponent transform;
            transform.setPosition({0.0f, -100.0f, 0.0f});
            transform.setScale({0.001f, 0.001f, 0.001f});
            rc->setTransform(transform);
            renderSystem->unbindEntity();
        }
    }

    if (_editorSystem)
    {
        _editorSystem->setLayout(40, kPanelWidth, kPanelHeight);
        _editorSystem->setFocused(false);
    }

    return vigine::Result();
}

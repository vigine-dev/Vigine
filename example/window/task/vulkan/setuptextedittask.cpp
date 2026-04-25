#include "setuptextedittask.h"

#include <vigine/context.h>
#include <vigine/impl/ecs/entitymanager.h>
#include <vigine/ecs/render/meshcomponent.h>
#include <vigine/ecs/render/rendercomponent.h>
#include <vigine/ecs/render/shadercomponent.h>
#include <vigine/ecs/render/textcomponent.h>
#include <vigine/ecs/render/transformcomponent.h>
#include <vigine/property.h>
#include <vigine/service/graphicsservice.h>

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

void SetupTextEditTask::contextChanged()
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

    if (_editorSystem)
        _editorSystem->bind(context(), _graphicsService,
                            _graphicsService ? _graphicsService->renderSystem() : nullptr);
}

vigine::Result SetupTextEditTask::execute()
{
    if (!_graphicsService)
        return vigine::Result(vigine::Result::Code::Error, "Graphics service is unavailable");

    if (!_state)
        return vigine::Result(vigine::Result::Code::Error, "TextEditState is null");

    const auto fontPath = resolveFontPath();
    if (fontPath.empty())
        return vigine::Result(vigine::Result::Code::Error,
                              "Font file not found (assets/fonts/segoeui.ttf)");

    auto *entityManager = context()->entityManager();

    // --- Background panel ---
    {
        auto *bgEntity = entityManager->createEntity();
        if (!bgEntity)
            return vigine::Result(vigine::Result::Code::Error, "Failed to create TextEditBgEntity");

        entityManager->addAlias(bgEntity, "TextEditBgEntity");
        _graphicsService->bindEntity(bgEntity);

        auto *rc = _graphicsService->renderComponent();
        if (!rc)
        {
            _graphicsService->unbindEntity();
            return vigine::Result(vigine::Result::Code::Error,
                                  "Render component unavailable for TextEditBgEntity");
        }

        // Flat dark-blue panel: slightly larger and moved toward camera (in front of cube).
        auto panelMesh = vigine::graphics::MeshComponent::createPlane(kPanelWidth, kPanelHeight,
                                                                      {0.08f, 0.08f, 0.25f});
        panelMesh.setProceduralInShader(true, 6); // Panel shader generates quad (6 vertices)
        rc->setMesh(panelMesh);
        {
            vigine::graphics::ShaderComponent shader("panel.vert.spv", "panel.frag.spv");
            rc->setShader(shader);
        }

        vigine::graphics::TransformComponent transform;
        transform.setPosition({0.0f, kPanelCenterY, kPanelZ});
        // Scale to actual panel dimensions so the unit-quad shader renders kPanelWidth x
        // kPanelHeight
        transform.setScale({kPanelWidth, kPanelHeight, 0.02f});
        rc->setTransform(transform);

        _graphicsService->unbindEntity();
    }

    // --- Vertical scrollbar track + thumb ---
    {
        auto *trackEntity = entityManager->createEntity();
        if (!trackEntity)
            return vigine::Result(vigine::Result::Code::Error,
                                  "Failed to create TextEditScrollbarTrackEntity");

        entityManager->addAlias(trackEntity, "TextEditScrollbarTrackEntity");
        _graphicsService->bindEntity(trackEntity);

        auto *rc = _graphicsService->renderComponent();
        if (!rc)
        {
            _graphicsService->unbindEntity();
            return vigine::Result(vigine::Result::Code::Error,
                                  "Render component unavailable for TextEditScrollbarTrackEntity");
        }

        auto trackMesh =
            vigine::graphics::MeshComponent::createPlane(1.0f, 1.0f, {0.18f, 0.18f, 0.32f});
        trackMesh.setProceduralInShader(true, 6);
        rc->setMesh(trackMesh);
        {
            vigine::graphics::ShaderComponent shader("panel.vert.spv", "panel.frag.spv");
            rc->setShader(shader);
        }

        vigine::graphics::TransformComponent transform;
        transform.setPosition({kScrollbarX, kPanelCenterY, kPanelZ + 0.005f});
        transform.setScale({kScrollbarWidth, kScrollbarHeight, 0.01f});
        rc->setTransform(transform);
        _graphicsService->unbindEntity();

        auto *thumbEntity = entityManager->createEntity();
        if (!thumbEntity)
            return vigine::Result(vigine::Result::Code::Error,
                                  "Failed to create TextEditScrollbarThumbEntity");

        entityManager->addAlias(thumbEntity, "TextEditScrollbarThumbEntity");
        _graphicsService->bindEntity(thumbEntity);

        rc = _graphicsService->renderComponent();
        if (!rc)
        {
            _graphicsService->unbindEntity();
            return vigine::Result(vigine::Result::Code::Error,
                                  "Render component unavailable for TextEditScrollbarThumbEntity");
        }

        auto thumbMesh =
            vigine::graphics::MeshComponent::createPlane(1.0f, 1.0f, {0.78f, 0.82f, 0.95f});
        thumbMesh.setProceduralInShader(true, 6);
        rc->setMesh(thumbMesh);
        {
            vigine::graphics::ShaderComponent shader("panel.vert.spv", "panel.frag.spv");
            rc->setShader(shader);
        }

        transform.setPosition({kScrollbarX, 2.14f, kPanelZ + 0.006f});
        transform.setScale({kScrollbarWidth * 0.82f, kScrollbarThumbHeight, 0.012f});
        rc->setTransform(transform);

        _graphicsService->unbindEntity();
    }

    // --- Editable text (bitmap-style with individual character planes) ---
    {
        auto *textEntity = entityManager->createEntity();
        if (!textEntity)
            return vigine::Result(vigine::Result::Code::Error, "Failed to create TextEditEntity");

        entityManager->addAlias(textEntity, "TextEditEntity");
        _graphicsService->bindEntity(textEntity);

        auto *rc = _graphicsService->renderComponent();
        if (!rc)
        {
            _graphicsService->unbindEntity();
            return vigine::Result(vigine::Result::Code::Error,
                                  "Render component unavailable for TextEditEntity");
        }

        vigine::graphics::TextComponent text;
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
        vigine::graphics::MeshComponent glyphMesh;
        glyphMesh.setProceduralInShader(true, 6); // 6 vertices per glyph quad instance
        rc->setMesh(glyphMesh);

        // Set shader before setText so that RenderComponent knows to build SDF quads.
        {
            vigine::graphics::ShaderComponent shader("glyph.vert.spv", "glyph.frag.spv");
            // Glyph shader generates procedural quad per glyph instance (6 vertices)
            shader.setInstancedRendering(true);
            // Instance vertex layout: mat4 as 4 consecutive vec4 attributes.
            vigine::graphics::VertexBindingDesc instBinding;
            instBinding.binding      = 0;
            instBinding.stride       = sizeof(glm::mat4);
            instBinding.instanceRate = true;
            instBinding.attributes   = {
                {0, vigine::graphics::VertexFormat::Float32x4, 0 },
                {1, vigine::graphics::VertexFormat::Float32x4, 16},
                {2, vigine::graphics::VertexFormat::Float32x4, 32},
                {3, vigine::graphics::VertexFormat::Float32x4, 48},
            };
            shader.setVertexLayout({instBinding});
            rc->setShader(shader);
        }

        if (!rc->setText(text))
        {
            _graphicsService->unbindEntity();
            return vigine::Result(vigine::Result::Code::Error,
                                  "Failed to build editor text voxels");
        }

        vigine::graphics::TransformComponent transform;
        // Entity stays at panel center (proven visible); anchorOffset shifts text to top-left.
        transform.setPosition({0.0f, kPanelCenterY, 1.21f});
        transform.setScale({1.0f, 1.0f, 1.0f});
        rc->setTransform(transform);

        _graphicsService->unbindEntity();
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
            auto *e = entityManager->createEntity();
            if (!e)
                return vigine::Result(vigine::Result::Code::Error,
                                      "Failed to create focus frame entity");

            entityManager->addAlias(e, alias);
            _graphicsService->bindEntity(e);

            auto *rc = _graphicsService->renderComponent();
            if (!rc)
            {
                _graphicsService->unbindEntity();
                return vigine::Result(vigine::Result::Code::Error,
                                      "Render component unavailable for focus frame entity");
            }

            auto frameMesh =
                vigine::graphics::MeshComponent::createPlane(1.0f, 1.0f, {1.0f, 1.0f, 1.0f});
            frameMesh.setProceduralInShader(true, 6);
            rc->setMesh(frameMesh);
            {
                vigine::graphics::ShaderComponent shader("panel.vert.spv", "panel.frag.spv");
                rc->setShader(shader);
            }

            vigine::graphics::TransformComponent transform;
            transform.setPosition({0.0f, -100.0f, 0.0f});
            transform.setScale({0.001f, 0.001f, 0.001f});
            rc->setTransform(transform);
            _graphicsService->unbindEntity();
        }
    }

    if (_editorSystem)
    {
        _editorSystem->setLayout(40, kPanelWidth, kPanelHeight);
        _editorSystem->setFocused(false);
    }

    return vigine::Result();
}

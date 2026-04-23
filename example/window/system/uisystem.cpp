#include "uisystem.h"

#include "../component/settingspanelcomponent.h"

#include <vigine/ecs/entitymanager.h>
#include <vigine/ecs/render/meshcomponent.h>
#include <vigine/ecs/render/rendercomponent.h>
#include <vigine/ecs/render/rendersystem.h>
#include <vigine/ecs/render/shadercomponent.h>
#include <vigine/ecs/render/textcomponent.h>
#include <vigine/ecs/render/transformcomponent.h>
#include <vigine/service/graphicsservice.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <glm/glm.hpp>

UISystem::UISystem(const vigine::SystemName& name,
                   vigine::EntityManager* entityManager,
                   vigine::graphics::GraphicsService* graphicsService)
    : vigine::AbstractSystem(name)
    , _entityManager(entityManager)
    , _graphicsService(graphicsService)
{
}

UISystem::~UISystem() = default;

// ── AbstractSystem interface ─────────────────────────────────────────────────

bool UISystem::hasComponents(vigine::Entity* entity) const
{
    return _groups.contains(entity) && !_groups.at(entity).empty();
}

void UISystem::createComponents(vigine::Entity* entity)
{
    // Called by ECS framework when entity binding is created.
    // Actual group creation is explicit via createGroup().
    static_cast<void>(entity);
}

void UISystem::destroyComponents(vigine::Entity* entity)
{
    _groups.erase(entity);
}

// ── Component Group API ──────────────────────────────────────────────────────

vigine::AbstractComponent* UISystem::createGroup(vigine::Entity* entity, UIGroupType groupType)
{
    if (!entity)
        return nullptr;

    // Destroy existing group of the same type first (idempotent).
    destroyGroup(entity, groupType);

    ComponentGroup group = dispatchBuild(entity, groupType);
    auto* token = group.token;
    _groups[entity].push_back(std::move(group));
    return token;
}

void UISystem::destroyGroup(vigine::Entity* entity, UIGroupType groupType)
{
    auto it = _groups.find(entity);
    if (it == _groups.end())
        return;

    auto& vec = it->second;
    vec.erase(std::remove_if(vec.begin(), vec.end(),
                             [groupType](const ComponentGroup& g) {
                                 return g.groupType == groupType;
                             }),
              vec.end());
}

vigine::AbstractComponent* UISystem::findGroup(vigine::Entity* entity,
                                                UIGroupType groupType) const
{
    const auto* g = findGroupInternal(entity, groupType);
    return g ? g->token : nullptr;
}

void UISystem::toggleGroup(vigine::Entity* entity, UIGroupType groupType)
{
    auto* g = findGroupInternal(entity, groupType);
    if (!g)
        return;
    g->visible = !g->visible;
    if (g->token)
    {
        if (auto* spc = dynamic_cast<SettingsPanelComponent*>(g->token))
        {
            spc->visible = g->visible;
            if (g->visible)
                showSettingsPanelEntities(spc);
            else
                hideSettingsPanelEntities(spc);
        }
    }
}

void UISystem::showGroup(vigine::Entity* entity, UIGroupType groupType)
{
    auto* g = findGroupInternal(entity, groupType);
    if (!g)
        return;
    g->visible = true;
    if (auto* spc = dynamic_cast<SettingsPanelComponent*>(g->token))
    {
        spc->visible = true;
        showSettingsPanelEntities(spc);
    }
}

void UISystem::hideGroup(vigine::Entity* entity, UIGroupType groupType)
{
    auto* g = findGroupInternal(entity, groupType);
    if (!g)
        return;
    g->visible = false;
    if (auto* spc = dynamic_cast<SettingsPanelComponent*>(g->token))
    {
        spc->visible = false;
        hideSettingsPanelEntities(spc);
    }
}

bool UISystem::isGroupVisible(vigine::Entity* entity, UIGroupType groupType) const
{
    const auto* g = findGroupInternal(entity, groupType);
    return g && g->visible;
}

bool UISystem::hasVisibleGroup(vigine::Entity* entity) const
{
    auto it = _groups.find(entity);
    if (it == _groups.end())
        return false;
    for (const auto& g : it->second)
    {
        if (g.visible)
            return true;
    }
    return false;
}

// ── Input ────────────────────────────────────────────────────────────────────

void UISystem::onKeyDown(vigine::Entity* entity, const vigine::platform::KeyEvent& event)
{
    auto it = _groups.find(entity);
    if (it == _groups.end())
        return;

    for (auto& group : it->second)
    {
        if (!group.visible)
            continue;

        switch (group.groupType)
        {
        case UIGroupType::SettingsPanel:
            handleSettingsPanelKeyDown(group, event);
            break;
        }
    }
}

void UISystem::onKeyUp(vigine::Entity* entity, const vigine::platform::KeyEvent& event)
{
    static_cast<void>(entity);
    static_cast<void>(event);
}

void UISystem::onMouseButtonDown(vigine::Entity* entity,
                                  vigine::platform::MouseButton button, int x, int y)
{
    static_cast<void>(button);
    auto it = _groups.find(entity);
    if (it == _groups.end())
        return;

    for (auto& group : it->second)
    {
        if (!group.visible)
            continue;

        switch (group.groupType)
        {
        case UIGroupType::SettingsPanel:
            handleSettingsPanelClick(group, x, y);
            break;
        }
    }
}

void UISystem::onMouseButtonUp(vigine::Entity* entity,
                                vigine::platform::MouseButton button, int x, int y)
{
    static_cast<void>(entity);
    static_cast<void>(button);
    static_cast<void>(x);
    static_cast<void>(y);
}

void UISystem::onMouseMove(vigine::Entity* entity, int x, int y)
{
    auto it = _groups.find(entity);
    if (it == _groups.end())
        return;

    for (auto& group : it->second)
    {
        if (!group.visible)
            continue;

        switch (group.groupType)
        {
        case UIGroupType::SettingsPanel:
            handleSettingsPanelMouseMove(group, x, y);
            break;
        }
    }
}

void UISystem::render(vigine::Entity* entity)
{
    auto it = _groups.find(entity);
    if (it == _groups.end())
        return;

    for (auto& group : it->second)
    {
        if (!group.visible)
            continue;

        switch (group.groupType)
        {
        case UIGroupType::SettingsPanel:
            renderSettingsPanel(group);
            break;
        }
    }
}

// ── Internal helpers ─────────────────────────────────────────────────────────

UISystem::ComponentGroup* UISystem::findGroupInternal(vigine::Entity* entity,
                                                       UIGroupType groupType)
{
    auto it = _groups.find(entity);
    if (it == _groups.end())
        return nullptr;

    for (auto& g : it->second)
    {
        if (g.groupType == groupType)
            return &g;
    }
    return nullptr;
}

const UISystem::ComponentGroup* UISystem::findGroupInternal(vigine::Entity* entity,
                                                              UIGroupType groupType) const
{
    auto it = _groups.find(entity);
    if (it == _groups.end())
        return nullptr;

    for (const auto& g : it->second)
    {
        if (g.groupType == groupType)
            return &g;
    }
    return nullptr;
}

// ── Profile list API ─────────────────────────────────────────────────────────

void UISystem::setProfiles(std::vector<vigine::platform::InputProfileComponent*> profiles)
{
    _profiles = std::move(profiles);
}

void UISystem::setActiveProfileIndex(int index)
{
    if (index >= 0 && index < static_cast<int>(_profiles.size()))
        _activeProfileIndex = index;
}

void UISystem::setProfileChangedCallback(
    std::function<void(vigine::platform::InputProfileComponent*)> cb)
{
    _onProfileChanged = std::move(cb);
}

// ── Build ────────────────────────────────────────────────────────────────────

UISystem::ComponentGroup UISystem::dispatchBuild(vigine::Entity* entity, UIGroupType groupType)
{
    switch (groupType)
    {
    case UIGroupType::SettingsPanel:
        return buildSettingsPanel(entity);
    }
    return {};
}

UISystem::ComponentGroup UISystem::buildSettingsPanel(vigine::Entity* entity)
{
    static_cast<void>(entity);

    ComponentGroup group;
    group.groupType = UIGroupType::SettingsPanel;
    group.visible = false;

    auto* token = new SettingsPanelComponent();
    group.token = token;

    createSettingsPanelEntities(token);

    return group;
}

// ── Settings Panel handlers ──────────────────────────────────────────────────

void UISystem::handleSettingsPanelKeyDown(ComponentGroup& group,
                                           const vigine::platform::KeyEvent& event)
{
    auto* spc = dynamic_cast<SettingsPanelComponent*>(group.token);
    if (!spc)
        return;

    constexpr unsigned int kEscape = 0x1B;
    constexpr unsigned int kUp     = 0x26;
    constexpr unsigned int kDown   = 0x28;
    constexpr unsigned int kEnter  = 0x0D;

    if (event.keyCode == kEscape)
    {
        group.visible = false;
        spc->visible = false;
        hideSettingsPanelEntities(spc);
        return;
    }

    if (_profiles.empty())
        return;

    const int count = static_cast<int>(_profiles.size());

    if (event.keyCode == kUp)
    {
        _activeProfileIndex = (_activeProfileIndex - 1 + count) % count;
        spc->selectedTabIndex = _activeProfileIndex;
        spc->textDirty = true;
        if (_onProfileChanged)
            _onProfileChanged(_profiles[static_cast<std::size_t>(_activeProfileIndex)]);
    }
    else if (event.keyCode == kDown)
    {
        _activeProfileIndex = (_activeProfileIndex + 1) % count;
        spc->selectedTabIndex = _activeProfileIndex;
        spc->textDirty = true;
        if (_onProfileChanged)
            _onProfileChanged(_profiles[static_cast<std::size_t>(_activeProfileIndex)]);
    }
    else if (event.keyCode == kEnter)
    {
        _activeProfileIndex = spc->selectedTabIndex;
        spc->textDirty = true;
        if (_onProfileChanged)
            _onProfileChanged(_profiles[static_cast<std::size_t>(_activeProfileIndex)]);
    }
}

// Panel layout constants (logical pixel offsets — used by click/hover until replaced with raycasting)
namespace
{
constexpr int kPanelOriginX   = 100;
constexpr int kPanelOriginY   = 80;
constexpr int kRowHeight      = 28;
constexpr int kPanelWidth     = 340;
constexpr int kHeaderRows     = 2;

// World-space constants for GPU panel entities
constexpr float kSettingsBgWidth   = 2.2f;
constexpr float kSettingsBgHeight  = 1.6f;
constexpr float kSettingsBgZ       = 0.8f;
constexpr float kSettingsTextZ     = 0.81f;
constexpr float kSettingsHiddenY   = -100.0f;
constexpr float kPanelDistance     = 2.0f;  // distance in front of camera

std::string resolveSettingsFontPath()
{
    for (const auto& c : {"assets/fonts/segoeui.ttf",
                          "example/window/assets/fonts/segoeui.ttf",
                          "../../example/window/assets/fonts/segoeui.ttf",
                          "C:/Windows/Fonts/segoeui.ttf"})
        if (std::filesystem::exists(c))
            return c;
    return {};
}
} // namespace

void UISystem::handleSettingsPanelClick(ComponentGroup& group, int x, int y)
{
    if (_profiles.empty() || !_graphicsService)
        return;

    auto* spc = dynamic_cast<SettingsPanelComponent*>(group.token);
    if (!spc)
        return;

    auto* rs = _graphicsService->renderSystem();
    if (!rs)
        return;

    // Cast ray from screen pixel
    glm::vec3 rayOrigin, rayDir;
    if (!rs->screenPointToRay(x, y, rayOrigin, rayDir))
        return;

    // Panel plane: center = camPos + camForward * distance, normal = camForward
    const glm::vec3 camForward = rs->cameraForwardDirection();
    const glm::vec3 panelCenter =
        rs->cameraPosition() + camForward * kPanelDistance;
    const glm::vec3 panelNormal = camForward;

    // Ray-plane intersection: t = dot(panelCenter - rayOrigin, normal) / dot(rayDir, normal)
    const float denom = glm::dot(rayDir, panelNormal);
    if (std::abs(denom) < 1e-6f)
        return;

    const float t = glm::dot(panelCenter - rayOrigin, panelNormal) / denom;
    if (t <= 0.0f)
        return;

    const glm::vec3 hitWorld = rayOrigin + rayDir * t;

    // Build panel-local coordinate system (same as updateSettingsPanelPosition)
    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    const glm::vec3 panelRight = glm::normalize(glm::cross(worldUp, panelNormal));
    const glm::vec3 panelUp    = glm::normalize(glm::cross(panelNormal, panelRight));

    // Local coords: origin = panel center, X = right, Y = up
    const glm::vec3 diff = hitWorld - panelCenter;
    const float localX = glm::dot(diff, panelRight);
    const float localY = glm::dot(diff, panelUp);

    // Check within panel bounds
    const float halfW = kSettingsBgWidth  * 0.5f;
    const float halfH = kSettingsBgHeight * 0.5f;
    if (localX < -halfW || localX > halfW || localY < -halfH || localY > halfH)
        return;

    // Map localY to row index.
    // Text uses topLeftAnchor with anchorOffset {-1.0, 0.65} in world-space units.
    // Renderer line height = pixelSize * 1.25 * voxelSize.
    constexpr float kVoxelSize   = 0.0024f;
    constexpr int   kPixelSize   = 28;
    constexpr float kLineHeight  = kPixelSize * 1.25f * kVoxelSize;
    constexpr int   kHeaderLines = 2; // "Keyboard Shortcuts Profile" + blank line
    constexpr float kTextTopY    = 0.65f; // anchorOffset.y (world-space)

    const float clickY = kTextTopY - localY; // distance from top (localY is up-positive)
    if (clickY < 0.0f)
        return;

    const int lineIndex = static_cast<int>(clickY / kLineHeight);
    const int row = lineIndex - kHeaderLines;
    if (row < 0 || row >= static_cast<int>(_profiles.size()))
        return;

    spc->selectedTabIndex = row;
    _activeProfileIndex   = row;
    spc->textDirty = true;

    if (_onProfileChanged)
        _onProfileChanged(_profiles[static_cast<std::size_t>(row)]);
}

void UISystem::handleSettingsPanelMouseMove(ComponentGroup& group, int x, int y)
{
    if (_profiles.empty() || !_graphicsService)
        return;

    auto* spc = dynamic_cast<SettingsPanelComponent*>(group.token);
    if (!spc)
        return;

    auto* rs = _graphicsService->renderSystem();
    if (!rs)
        return;

    glm::vec3 rayOrigin, rayDir;
    if (!rs->screenPointToRay(x, y, rayOrigin, rayDir))
        return;

    const glm::vec3 camForward = rs->cameraForwardDirection();
    const glm::vec3 panelCenter =
        rs->cameraPosition() + camForward * kPanelDistance;

    const float denom = glm::dot(rayDir, camForward);
    int newHovered = -1;

    if (std::abs(denom) > 1e-6f)
    {
        const float t = glm::dot(panelCenter - rayOrigin, camForward) / denom;
        if (t > 0.0f)
        {
            const glm::vec3 hitWorld = rayOrigin + rayDir * t;
            const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
            const glm::vec3 panelRight = glm::normalize(glm::cross(worldUp, camForward));
            const glm::vec3 panelUp    = glm::normalize(glm::cross(camForward, panelRight));

            const glm::vec3 diff = hitWorld - panelCenter;
            const float localX = glm::dot(diff, panelRight);
            const float localY = glm::dot(diff, panelUp);

            const float halfW = kSettingsBgWidth  * 0.5f;
            const float halfH = kSettingsBgHeight * 0.5f;

            if (localX >= -halfW && localX <= halfW &&
                localY >= -halfH && localY <= halfH)
            {
                constexpr float kVoxelSize   = 0.0024f;
                constexpr int   kPixelSize   = 28;
                constexpr float kLineHeight  = kPixelSize * 1.25f * kVoxelSize;
                constexpr int   kHeaderLines = 2;
                constexpr float kTextTopY    = 0.65f;

                const float clickY = kTextTopY - localY;
                if (clickY >= 0.0f)
                {
                    const int lineIndex = static_cast<int>(clickY / kLineHeight);
                    const int row = lineIndex - kHeaderLines;
                    if (row >= 0 && row < static_cast<int>(_profiles.size()))
                        newHovered = row;
                }
            }
        }
    }

    if (newHovered != spc->hoveredProfileIndex)
    {
        spc->hoveredProfileIndex = newHovered;
        spc->textDirty = true;
    }
}

void UISystem::renderSettingsPanel(ComponentGroup& group)
{
    auto* spc = dynamic_cast<SettingsPanelComponent*>(group.token);
    if (!spc || !spc->visible)
        return;

    updateSettingsPanelPosition(spc);

    // SDF vertices are baked in world-space using the entity's model matrix.
    // Since the panel follows the camera each frame, we must rebuild every frame.
    rebuildSettingsPanelSdf(spc);
    spc->textDirty = false;
}

// ── Settings Panel entity helpers ────────────────────────────────────────────

void UISystem::createSettingsPanelEntities(SettingsPanelComponent* spc)
{
    if (!spc || !_entityManager || !_graphicsService)
        return;

    // --- Background panel ---
    {
        auto* bgEntity = _entityManager->createEntity();
        if (!bgEntity)
            return;

        spc->bgEntity = bgEntity;
        _graphicsService->bindEntity(bgEntity);

        auto* rc = _graphicsService->renderComponent();
        if (!rc)
        {
            _graphicsService->unbindEntity();
            return;
        }

        auto panelMesh = vigine::graphics::MeshComponent::createPlane(
            kSettingsBgWidth, kSettingsBgHeight, {0.06f, 0.06f, 0.18f});
        panelMesh.setProceduralInShader(true, 6);
        rc->setMesh(panelMesh);

        vigine::graphics::ShaderComponent shader("panel.vert.spv", "panel.frag.spv");
        rc->setShader(shader);

        vigine::graphics::TransformComponent transform;
        transform.setPosition({0.0f, kSettingsHiddenY, kSettingsBgZ});
        transform.setScale({kSettingsBgWidth, kSettingsBgHeight, 0.02f});
        rc->setTransform(transform);

        _graphicsService->unbindEntity();
    }

    // --- SDF text ---
    {
        const auto fontPath = resolveSettingsFontPath();
        if (fontPath.empty())
        {
            std::cerr << "[UISystem] Settings panel: font file not found" << std::endl;
            return;
        }

        auto* textEntity = _entityManager->createEntity();
        if (!textEntity)
            return;

        spc->textEntity = textEntity;
        _graphicsService->bindEntity(textEntity);

        auto* rc = _graphicsService->renderComponent();
        if (!rc)
        {
            _graphicsService->unbindEntity();
            return;
        }

        vigine::graphics::TextComponent text;
        text.setEnabled(true);
        text.setDrawBaseInstance(false);
        text.setText("Settings");
        text.setCursorVisible(false);
        text.setFontPath(fontPath);
        text.setPixelSize(28);
        text.setVoxelSize(0.0024f);
        text.setTopLeftAnchor(true);
        text.setAnchorOffset({-1.0f, 0.65f});
        text.setMaxLineWorldWidth(2.0f);

        vigine::graphics::MeshComponent glyphMesh;
        glyphMesh.setProceduralInShader(true, 6);
        rc->setMesh(glyphMesh);

        {
            vigine::graphics::ShaderComponent shader("glyph.vert.spv", "glyph.frag.spv");
            shader.setInstancedRendering(true);
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
            std::cerr << "[UISystem] Settings panel: failed to build SDF text" << std::endl;
            _graphicsService->unbindEntity();
            return;
        }

        vigine::graphics::TransformComponent transform;
        transform.setPosition({0.0f, kSettingsHiddenY, kSettingsTextZ});
        transform.setScale({1.0f, 1.0f, 1.0f});
        rc->setTransform(transform);

        _graphicsService->unbindEntity();
    }
}

void UISystem::showSettingsPanelEntities(SettingsPanelComponent* spc)
{
    if (!spc || !_graphicsService)
        return;

    updateSettingsPanelPosition(spc);
    spc->textDirty = true;
}

void UISystem::hideSettingsPanelEntities(SettingsPanelComponent* spc)
{
    if (!spc || !_graphicsService)
        return;

    const glm::vec3 hidden{0.0f, kSettingsHiddenY, 0.0f};

    if (spc->bgEntity)
    {
        _graphicsService->bindEntity(spc->bgEntity);
        if (auto* rc = _graphicsService->renderComponent())
        {
            auto tr = rc->getTransform();
            tr.setPosition(hidden);
            rc->setTransform(tr);
        }
        _graphicsService->unbindEntity();
    }

    if (spc->textEntity)
    {
        _graphicsService->bindEntity(spc->textEntity);
        if (auto* rc = _graphicsService->renderComponent())
        {
            auto tr = rc->getTransform();
            tr.setPosition(hidden);
            rc->setTransform(tr);
            // Re-bake SDF vertices at hidden position so glyphs actually disappear
            static_cast<void>(rc->refreshSdfGlyphQuads());
            if (auto* rs = _graphicsService->renderSystem())
                rs->markGlyphDirty();
        }
        _graphicsService->unbindEntity();
    }
}

void UISystem::updateSettingsPanelPosition(SettingsPanelComponent* spc)
{
    if (!spc || !_graphicsService)
        return;

    auto* rs = _graphicsService->renderSystem();
    if (!rs)
        return;

    const glm::vec3 camPos     = rs->cameraPosition();
    const glm::vec3 camForward = rs->cameraForwardDirection();
    const glm::vec3 center     = camPos + camForward * kPanelDistance;

    // Exact euler decomposition for X→Y→Z rotation order used by TransformComponent.
    // Makes the panel perfectly perpendicular to the camera view direction.
    const float p    = std::asin(glm::clamp(camForward.y, -1.0f, 1.0f));
    const float y    = std::atan2(camForward.x, -camForward.z);
    const float cosP = std::cos(p);
    const float sinP = std::sin(p);
    const float cosY = std::cos(y);
    const float sinY = std::sin(y);

    const float rx = std::atan2(sinP, cosP * cosY);
    const float ry = std::asin(glm::clamp(-cosP * sinY, -1.0f, 1.0f));
    const float rz = std::atan2(sinY * sinP, cosY);

    if (spc->bgEntity)
    {
        _graphicsService->bindEntity(spc->bgEntity);
        if (auto* rc = _graphicsService->renderComponent())
        {
            auto tr = rc->getTransform();
            tr.setPosition(center);
            tr.setRotation({rx, ry, rz});
            rc->setTransform(tr);
        }
        _graphicsService->unbindEntity();
    }

    if (spc->textEntity)
    {
        _graphicsService->bindEntity(spc->textEntity);
        if (auto* rc = _graphicsService->renderComponent())
        {
            const glm::vec3 textPos = center - camForward * 0.02f;
            auto tr = rc->getTransform();
            tr.setPosition(textPos);
            tr.setRotation({rx, ry, rz});
            rc->setTransform(tr);
        }
        _graphicsService->unbindEntity();
    }
}

std::string UISystem::buildSettingsPanelText(int hoveredIndex) const
{
    std::ostringstream oss;
    oss << "Keyboard Shortcuts Profile\n\n";

    for (int i = 0; i < static_cast<int>(_profiles.size()); ++i)
    {
        const char* marker =
            (i == _activeProfileIndex) ? "> " :
            (i == hoveredIndex)        ? "- " : "  ";
        oss << marker << _profiles[static_cast<std::size_t>(i)]->name() << "\n";
    }

    return oss.str();
}

void UISystem::rebuildSettingsPanelSdf(SettingsPanelComponent* spc)
{
    if (!spc || !spc->textEntity || !_graphicsService)
        return;

    _graphicsService->bindEntity(spc->textEntity);
    if (auto* rc = _graphicsService->renderComponent())
    {
        if (spc->textDirty)
        {
            // Text content changed — full rebuild
            const std::string content = buildSettingsPanelText(spc->hoveredProfileIndex);
            auto& text = rc->getText();
            text.setText(content);
            static_cast<void>(rc->incrementalRebuildSdf(0));
        }
        else
        {
            // Only transform changed — re-emit world vertices from cached layout
            static_cast<void>(rc->refreshSdfGlyphQuads());
        }

        if (auto* rs = _graphicsService->renderSystem())
            rs->markGlyphDirty();
    }
    _graphicsService->unbindEntity();
}

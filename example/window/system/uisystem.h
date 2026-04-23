#pragma once

#include <vigine/ecs/abstractsystem.h>
#include <vigine/ecs/abstractcomponent.h>
#include <vigine/ecs/platform/iwindoweventhandler.h>
#include <vigine/ecs/platform/inputprofilecomponent.h>

#include <functional>
#include <unordered_map>
#include <vector>

namespace vigine { class EntityManager; }
namespace vigine::graphics { class GraphicsService; }

class SettingsPanelComponent;

// Types of UI groups — extend this enum when adding new UI component types.
enum class UIGroupType
{
    SettingsPanel,
    // Button,
    // TextEditor,
    // Slider,
};

class UISystem : public vigine::AbstractSystem
{
  public:
    UISystem(const vigine::SystemName& name,
             vigine::EntityManager* entityManager,
             vigine::graphics::GraphicsService* graphicsService);
    ~UISystem() override;

    // AbstractSystem interface
    [[nodiscard]] vigine::SystemId id() const override { return "UI"; }
    [[nodiscard]] bool hasComponents(vigine::Entity* entity) const override;
    void createComponents(vigine::Entity* entity) override;
    void destroyComponents(vigine::Entity* entity) override;

    // ── Generic Component Group API ──────────────────────────────────────────

    vigine::AbstractComponent* createGroup(vigine::Entity* entity, UIGroupType groupType);
    void destroyGroup(vigine::Entity* entity, UIGroupType groupType);
    vigine::AbstractComponent* findGroup(vigine::Entity* entity, UIGroupType groupType) const;

    // Visibility
    void toggleGroup(vigine::Entity* entity, UIGroupType groupType);
    void showGroup(vigine::Entity* entity, UIGroupType groupType);
    void hideGroup(vigine::Entity* entity, UIGroupType groupType);
    bool isGroupVisible(vigine::Entity* entity, UIGroupType groupType) const;
    bool hasVisibleGroup(vigine::Entity* entity) const;

    // ── Input / Render ───────────────────────────────────────────────────────

    void onKeyDown(vigine::Entity* entity, const vigine::platform::KeyEvent& event);
    void onKeyUp(vigine::Entity* entity, const vigine::platform::KeyEvent& event);
    void onMouseButtonDown(vigine::Entity* entity,
                           vigine::platform::MouseButton button, int x, int y);
    void onMouseButtonUp(vigine::Entity* entity,
                         vigine::platform::MouseButton button, int x, int y);
    void onMouseMove(vigine::Entity* entity, int x, int y);
    void render(vigine::Entity* entity);

    // ── Profile list for Settings panel ──────────────────────────────────────
    void setProfiles(std::vector<vigine::platform::InputProfileComponent*> profiles);
    void setActiveProfileIndex(int index);
    void setProfileChangedCallback(
        std::function<void(vigine::platform::InputProfileComponent*)> cb);

  private:
    struct ComponentGroup
    {
        vigine::AbstractComponent* token{nullptr};
        std::vector<vigine::AbstractComponent*> children;
        UIGroupType groupType{};
        bool visible{false};
    };

    std::unordered_map<vigine::Entity*, std::vector<ComponentGroup>> _groups;

    ComponentGroup* findGroupInternal(vigine::Entity* entity, UIGroupType groupType);
    const ComponentGroup* findGroupInternal(vigine::Entity* entity,
                                            UIGroupType groupType) const;

    // ── Build functions ──────────────────────────────────────────────────────
    ComponentGroup dispatchBuild(vigine::Entity* entity, UIGroupType groupType);
    ComponentGroup buildSettingsPanel(vigine::Entity* entity);

    // ── Input handlers ───────────────────────────────────────────────────────
    void handleSettingsPanelKeyDown(ComponentGroup& group,
                                    const vigine::platform::KeyEvent& event);
    void handleSettingsPanelClick(ComponentGroup& group, int x, int y);
    void handleSettingsPanelMouseMove(ComponentGroup& group, int x, int y);

    // ── Render handlers ──────────────────────────────────────────────────────
    void renderSettingsPanel(ComponentGroup& group);

    // ── Entity helpers ───────────────────────────────────────────────────────
    void createSettingsPanelEntities(SettingsPanelComponent* spc);
    void showSettingsPanelEntities(SettingsPanelComponent* spc);
    void hideSettingsPanelEntities(SettingsPanelComponent* spc);
    void updateSettingsPanelPosition(SettingsPanelComponent* spc);
    std::string buildSettingsPanelText(int hoveredIndex) const;
    void rebuildSettingsPanelSdf(SettingsPanelComponent* spc);

    vigine::EntityManager* _entityManager{nullptr};
    vigine::graphics::GraphicsService* _graphicsService{nullptr};

    // Profile data for Settings panel
    std::vector<vigine::platform::InputProfileComponent*> _profiles;
    int _activeProfileIndex{0};
    std::function<void(vigine::platform::InputProfileComponent*)> _onProfileChanged;
};

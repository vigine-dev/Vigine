#pragma once

#include <vigine/ecs/abstractcomponent.h>

namespace vigine { class Entity; }

// Token component: marker for the Settings Panel group.
// Contains no logic — only minimal state that UISystem reads/writes.
class SettingsPanelComponent : public vigine::AbstractComponent
{
  public:
    SettingsPanelComponent() = default;
    ~SettingsPanelComponent() override = default;

    bool visible{false};
    int selectedTabIndex{0};
    int hoveredProfileIndex{-1};

    // GPU entities created by UISystem
    vigine::Entity* bgEntity{nullptr};
    vigine::Entity* textEntity{nullptr};
    bool textDirty{true};
};

#pragma once

/**
 * @file windowsystem.h
 * @brief ECS system that creates and manages platform window components.
 */

#include "iwindoweventhandler.h"

#include "vigine/base/macros.h"
#include "vigine/ecs/abstractsystem.h"
#include "vigine/result.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace vigine
{
namespace platform
{
class WindowComponent;
class WindowEventDispatcher;

/**
 * @brief Owns WindowComponent instances and routes events to handlers.
 *
 * Derived from AbstractSystem. Creates windows on demand, binds them
 * to an Entity, shows them, and attaches IWindowEventHandlerComponent
 * callbacks via an internal WindowEventDispatcher. Exposes read-only
 * lookups for the windows and handlers owned by a given Entity.
 */
class WindowSystem : public AbstractSystem
{
  public:
    using WindowComponentUPtr    = std::unique_ptr<WindowComponent>;
    using WindowComponentList    = std::vector<WindowComponentUPtr>;
    using WindowComponentPtrList = std::vector<WindowComponent *>;

    WindowSystem(const SystemName &name);
    ~WindowSystem() override;

    // interface implementation
    [[nodiscard]] SystemId id() const override;
    [[nodiscard]] bool hasComponents(Entity *entity) const override;
    void createComponents(Entity *entity) override;
    void destroyComponents(Entity *entity) override;

    // custom methods
    [[nodiscard]] WindowComponent *createWindowComponent();
    [[nodiscard]] Result bindWindowComponent(Entity *entity, WindowComponent *window);
    [[nodiscard]] Result showWindow(WindowComponent *window);
    [[nodiscard]] Result bindWindowEventHandler(Entity *entity, WindowComponent *window,
                                                IWindowEventHandlerComponent *handler);
    [[nodiscard]] WindowComponentPtrList windowComponents(Entity *entity) const;
    [[nodiscard]] std::vector<IWindowEventHandlerComponent *>
    windowEventHandlers(Entity *entity, WindowComponent *window) const;

  protected:
    void entityBound() override;
    void entityUnbound() override;

  private:
    WindowComponentList _windowComponents;
    std::unordered_map<WindowComponent *, std::unique_ptr<WindowEventDispatcher>> _dispatchers;
    std::unordered_map<Entity *, std::vector<WindowComponent *>> _entityWindows;
};

BUILD_SMART_PTR(WindowSystem);

} // namespace platform
} // namespace vigine

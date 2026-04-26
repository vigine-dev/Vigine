#include "vigine/impl/ecs/platform/windowsystem.h"
#include "windowcomponent.h"
#include "windoweventdispatcher.h"

#ifdef _WIN32
#include "winapicomponent.h"
#elif defined(__APPLE__)
#include "cocoawindowcomponent.h"
#endif

#include <vigine/result.h>

#include <algorithm>

using namespace vigine::ecs::platform;

WindowSystem::WindowSystem(const SystemName &name) : AbstractSystem(name) {}

WindowSystem::~WindowSystem() = default;

vigine::SystemId WindowSystem::id() const { return "Window"; }

bool WindowSystem::hasComponents(Entity *entity) const {
  if (!entity || _entityWindows.empty())
    return false;

  auto it = _entityWindows.find(entity);
  return it != _entityWindows.end() && !it->second.empty();
}

WindowComponent *WindowSystem::createWindowComponent() {
#ifdef _WIN32
  auto component = std::make_unique<WinAPIComponent>();
#elif defined(__APPLE__)
  auto component = std::make_unique<CocoaWindowComponent>();
#else
  auto component = std::make_unique<WindowComponent>();
#endif

  auto *rawPtr = component.get();
  _windowComponents.push_back(std::move(component));
  return rawPtr;
}

vigine::Result WindowSystem::bindWindowComponent(Entity *entity,
                                                 WindowComponent *window) {
  if (!entity || !window)
    return vigine::Result(vigine::Result::Code::Error,
                          "Invalid pointer provided");

  const bool isOwned =
      std::any_of(_windowComponents.begin(), _windowComponents.end(),
                  [window](const auto &c) { return c.get() == window; });
  if (!isOwned)
    return vigine::Result(vigine::Result::Code::Error,
                          "WindowComponent is not registered in this system");

  _entityWindows[entity].push_back(window);

  auto [dispatcherIt, inserted] =
      _dispatchers.emplace(window, std::make_unique<WindowEventDispatcher>());
  window->setEventHandler(dispatcherIt->second.get());

  return vigine::Result();
}

void WindowSystem::createComponents(Entity *entity) {
  if (!entity)
    return;

  auto *window = createWindowComponent();
  static_cast<void>(bindWindowComponent(entity, window));
}

void WindowSystem::destroyComponents(Entity *entity) {
  if (!entity)
    return;

  const auto it = _entityWindows.find(entity);
  if (it == _entityWindows.end())
    return;

  for (WindowComponent *window : it->second) {
    _dispatchers.erase(window);

    auto compIt =
        std::find_if(_windowComponents.begin(), _windowComponents.end(),
                     [window](const auto &c) { return c.get() == window; });
    if (compIt != _windowComponents.end())
      _windowComponents.erase(compIt);
  }

  _entityWindows.erase(entity);
}

vigine::Result WindowSystem::showWindow(WindowComponent *window) {
  if (!window)
    return vigine::Result(vigine::Result::Code::Error,
                          "Window component is null");

  const bool isOwned =
      std::any_of(_windowComponents.begin(), _windowComponents.end(),
                  [window](const auto &c) { return c.get() == window; });
  if (!isOwned)
    return vigine::Result(vigine::Result::Code::Error,
                          "WindowComponent is not registered in this system");

  window->show();
  return vigine::Result();
}
vigine::Result
WindowSystem::bindWindowEventHandler(Entity *entity, WindowComponent *window,
                                     IWindowEventHandlerComponent *handler) {
  if (!entity || !window || !handler)
    return vigine::Result(vigine::Result::Code::Error,
                          "Invalid pointer provided");

  const bool isOwned =
      std::any_of(_windowComponents.begin(), _windowComponents.end(),
                  [window](const auto &c) { return c.get() == window; });
  if (!isOwned)
    return vigine::Result(vigine::Result::Code::Error,
                          "WindowComponent is not registered in this system");

  const auto entityIt = _entityWindows.find(entity);
  if (entityIt == _entityWindows.end())
    return vigine::Result(
        vigine::Result::Code::Error,
        "WindowComponent is not registered for the provided entity");

  const auto &windows = entityIt->second;
  if (std::find(windows.begin(), windows.end(), window) == windows.end())
    return vigine::Result(
        vigine::Result::Code::Error,
        "WindowComponent is not registered for the provided entity");

  const auto dispIt = _dispatchers.find(window);
  if (dispIt == _dispatchers.end())
    return vigine::Result(vigine::Result::Code::Error,
                          "No dispatcher found for the provided window");

  dispIt->second->addHandler(handler);

  return vigine::Result();
}

WindowSystem::WindowComponentPtrList
WindowSystem::windowComponents(Entity *entity) const {
  if (!entity)
    return {};

  const auto it = _entityWindows.find(entity);
  if (it == _entityWindows.end())
    return {};

  WindowComponentPtrList windows;
  windows.reserve(it->second.size());
  for (WindowComponent *window : it->second) {
    if (window)
      windows.push_back(window);
  }
  return windows;
}

std::vector<IWindowEventHandlerComponent *>
WindowSystem::windowEventHandlers(Entity *entity,
                                  WindowComponent *window) const {
  if (!entity)
    return {};

  const auto entityIt = _entityWindows.find(entity);
  if (entityIt == _entityWindows.end())
    return {};

  if (window) {
    const auto dispIt = _dispatchers.find(window);
    if (dispIt == _dispatchers.end())
      return {};
    return dispIt->second->handlers();
  }

  std::vector<IWindowEventHandlerComponent *> allHandlers;
  for (WindowComponent *win : entityIt->second) {
    const auto dispIt = _dispatchers.find(win);
    if (dispIt != _dispatchers.end()) {
      const auto &h = dispIt->second->handlers();
      allHandlers.insert(allHandlers.end(), h.begin(), h.end());
    }
  }
  return allHandlers;
}

void WindowSystem::entityBound() {
  const auto it = _entityWindows.find(getBoundEntity());
  if (it == _entityWindows.end() || it->second.empty())
    return;

  for (WindowComponent *window : it->second) {
    if (!window)
      continue;

    const auto dispIt = _dispatchers.find(window);
    if (dispIt != _dispatchers.end())
      window->setEventHandler(dispIt->second.get());
  }
}

void WindowSystem::entityUnbound() {}

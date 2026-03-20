#pragma once

namespace vigine
{
namespace platform
{
class IWindowEventHandlerComponent;
class WindowSystem;

class WindowComponent
{
  public:
    WindowComponent();
    virtual ~WindowComponent();

    virtual void setEventHandler(IWindowEventHandlerComponent *handler);
    IWindowEventHandlerComponent *_eventHandler{nullptr};

  protected:
    virtual void show();

    friend class WindowSystem;

  private:
    // X11Window _window;
    // X11Data _x11Data;
};
} // namespace platform
} // namespace vigine

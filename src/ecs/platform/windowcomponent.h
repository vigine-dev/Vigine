#pragma once

namespace vigine
{
namespace platform
{
class IWindowEventHandler;

class WindowComponent
{
  public:
    WindowComponent();
    virtual ~WindowComponent();

    virtual void show();
    virtual void setEventHandler(IWindowEventHandler *handler);
    IWindowEventHandler *_eventHandler{nullptr};

  private:
    // X11Window _window;
    // X11Data _x11Data;
};
} // namespace platform
} // namespace vigine

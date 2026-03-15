#pragma once

namespace vigine
{
namespace platform
{
class WindowComponent
{
  public:
    WindowComponent();
    virtual ~WindowComponent();

    virtual void show();

  private:
    // X11Window _window;
    // X11Data _x11Data;
};
} // namespace platform
} // namespace vigine

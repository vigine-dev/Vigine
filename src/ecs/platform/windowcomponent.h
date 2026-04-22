#pragma once

#include <functional>

namespace vigine
{
namespace platform
{
class IWindowEventHandlerComponent;
class WindowSystem;

class WindowComponent // ENCAP EXEMPTION: legacy; public _eventHandler pending cleanup
{
  public:
    WindowComponent();
    virtual ~WindowComponent();

    virtual void setEventHandler(IWindowEventHandlerComponent *handler);
    virtual void setFrameCallback(std::function<void()> callback);
    virtual void runFrameCallback();
    [[nodiscard]] virtual void *nativeHandle() const;
    IWindowEventHandlerComponent *_eventHandler{nullptr};

  protected:
    virtual void show();

    friend class WindowSystem;

  private:
    std::function<void()> _frameCallback;

    // X11Window _window;
    // X11Data _x11Data;

  protected:
    const std::function<void()> &frameCallback() const;
};
} // namespace platform
} // namespace vigine

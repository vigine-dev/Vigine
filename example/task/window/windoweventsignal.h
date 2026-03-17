#pragma once

#include <vigine/ecs/platform/iwindoweventhandler.h>
#include <vigine/signal/isignal.h>
#include <vigine/signal/isignalbinder.h>
#include <vigine/signal/isignalemiter.h>

namespace vigine
{
class AbstractTask;
}

class MouseButtonDownSignal : public vigine::ISignal
{
  public:
    MouseButtonDownSignal(vigine::platform::MouseButton button, int x, int y)
        : _button(button), _x(x), _y(y)
    {
    }

    ~MouseButtonDownSignal() override = default;

    [[nodiscard]] vigine::SignalType type() const override { return vigine::SignalType::Event; }

    [[nodiscard]] vigine::platform::MouseButton button() const { return _button; }
    [[nodiscard]] int x() const { return _x; }
    [[nodiscard]] int y() const { return _y; }

  private:
    vigine::platform::MouseButton _button;
    int _x;
    int _y;
};

class KeyDownSignal : public vigine::ISignal
{
  public:
    explicit KeyDownSignal(vigine::platform::KeyEvent event) : _event(event) {}

    ~KeyDownSignal() override = default;

    [[nodiscard]] vigine::SignalType type() const override { return vigine::SignalType::Event; }

    [[nodiscard]] const vigine::platform::KeyEvent &event() const { return _event; }

  private:
    vigine::platform::KeyEvent _event;
};

class IMouseEventSignalEmiter : public vigine::ISignalEmiter
{
  public:
    ~IMouseEventSignalEmiter() override = default;

  protected:
    void emitMouseButtonDownSignal(vigine::platform::MouseButton button, int x, int y)
    {
        if (proxyEmiter())
            proxyEmiter()(new MouseButtonDownSignal(button, x, y));
    }
};

class IKeyEventSignalEmiter : public vigine::ISignalEmiter
{
  public:
    ~IKeyEventSignalEmiter() override = default;

  protected:
    void emitKeyDownSignal(const vigine::platform::KeyEvent &event)
    {
        if (proxyEmiter())
            proxyEmiter()(new KeyDownSignal(event)); // VG_TODO: use move semantics and unique_ptr
    }
};

class IMouseEventSignalHandler
{
  public:
    virtual ~IMouseEventSignalHandler()                                = default;
    virtual void onMouseButtonDownSignal(MouseButtonDownSignal *event) = 0;
};

class IKeyEventSignalHandler
{
  public:
    virtual ~IKeyEventSignalHandler()                  = default;
    virtual void onKeyDownSignal(KeyDownSignal *event) = 0;
};

class MouseEventSignalBinder : public vigine::ISignalBinder
{
  public:
    MouseEventSignalBinder()           = default;
    ~MouseEventSignalBinder() override = default;

    [[nodiscard]] bool check(vigine::AbstractTask *taskEmiter,
                             vigine::AbstractTask *taskReceiver) override;

  private:
    IMouseEventSignalHandler *_taskReceiver{nullptr};
};

class KeyEventSignalBinder : public vigine::ISignalBinder
{
  public:
    KeyEventSignalBinder()           = default;
    ~KeyEventSignalBinder() override = default;

    [[nodiscard]] bool check(vigine::AbstractTask *taskEmiter,
                             vigine::AbstractTask *taskReceiver) override;

  private:
    IKeyEventSignalHandler *_taskReceiver{nullptr};
};

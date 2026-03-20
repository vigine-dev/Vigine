#include "windoweventsignal.h"

bool MouseEventSignalBinder::check(vigine::AbstractTask *taskEmiter,
                                   vigine::AbstractTask *taskReceiver)
{
    auto *mouseEventEmiter   = dynamic_cast<IMouseEventSignalEmiter *>(taskEmiter);
    auto *mouseEventReceiver = dynamic_cast<IMouseEventSignalHandler *>(taskReceiver);

    if (!mouseEventEmiter || !mouseEventReceiver)
        return false;

    auto proxyEmiter = [this](vigine::ISignal *signal) {
        if (!signal)
            return;

        auto *mouseEvent = dynamic_cast<MouseButtonDownSignal *>(signal);
        if (mouseEvent && _taskReceiver)
            _taskReceiver->onMouseButtonDownSignal(mouseEvent);

        delete signal;
    };

    mouseEventEmiter->setProxyEmiter(proxyEmiter);
    _taskReceiver = mouseEventReceiver;

    return true;
}

bool KeyEventSignalBinder::check(vigine::AbstractTask *taskEmiter,
                                 vigine::AbstractTask *taskReceiver)
{
    auto *keyEventEmiter   = dynamic_cast<IKeyEventSignalEmiter *>(taskEmiter);
    auto *keyEventReceiver = dynamic_cast<IKeyEventSignalHandler *>(taskReceiver);

    if (!keyEventEmiter || !keyEventReceiver)
        return false;

    auto proxyEmiter = [this](vigine::ISignal *signal) {
        if (!signal)
            return;

        auto *keyEvent = dynamic_cast<KeyDownSignal *>(signal);
        if (keyEvent && _taskReceiver)
            _taskReceiver->onKeyDownSignal(keyEvent);

        delete signal;
    };

    keyEventEmiter->setProxyEmiter(proxyEmiter);
    _taskReceiver = keyEventReceiver;

    return true;
}

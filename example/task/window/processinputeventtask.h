#pragma once

#include "windoweventsignal.h"

#include <vigine/abstracttask.h>


class ProcessInputEventTask : public vigine::AbstractTask,
                              public IMouseEventSignalHandler,
                              public IKeyEventSignalHandler
{
  public:
    ProcessInputEventTask();

    vigine::Result execute() override;

    void onMouseButtonDownSignal(MouseButtonDownSignal *event) override;
    void onKeyDownSignal(KeyDownSignal *event) override;

  private:
    bool _hasMouseEvent{false};
    bool _hasKeyEvent{false};
};

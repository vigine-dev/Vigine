#include "processinputeventtask.h"

#include <iostream>

ProcessInputEventTask::ProcessInputEventTask() = default;

// COPILOT_TODO: Або обробляти накопичені події та скидати _hasMouseEvent/_hasKeyEvent, або прибрати
// цю задачу; зараз execute() нічого не робить.
vigine::Result ProcessInputEventTask::execute() { return vigine::Result(); }

void ProcessInputEventTask::onMouseButtonDownSignal(MouseButtonDownSignal *event)
{
    if (!event)
        return;

    _hasMouseEvent = true;

    std::cout << "[ProcessInputEventTask::onMouseButtonDownSignal] Mouse button down: "
              << static_cast<int>(event->button()) << " at " << event->x() << ", " << event->y()
              << std::endl;
}

void ProcessInputEventTask::onKeyDownSignal(KeyDownSignal *event)
{
    if (!event)
        return;

    _hasKeyEvent = true;

    std::cout << "[ProcessInputEventTask::onKeyDownSignal] Key down: code="
              << event->event().keyCode << ", scan=" << event->event().scanCode
              << ", mods=" << event->event().modifiers
              << ", repeatCount=" << event->event().repeatCount
              << ", isRepeat=" << event->event().isRepeat << std::endl;
}

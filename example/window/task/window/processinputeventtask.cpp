#include "processinputeventtask.h"

#include <iostream>
#include <utility>

#include <vigine/api/messaging/imessage.h>

ProcessInputEventTask::ProcessInputEventTask() = default;

ProcessInputEventTask::~ProcessInputEventTask()
{
    // Belt-and-braces teardown: drop subscription tokens first so the bus
    // stops routing messages here and any in-flight onMessage drains,
    // before the rest of the state (still available below the clear call)
    // is destroyed. Reverse-declaration-order would take care of this on
    // its own because _tokens is the last member, but an explicit clear
    // keeps the contract obvious even if a future member is appended
    // after _tokens by accident.
    _tokens.clear();
}

vigine::Result ProcessInputEventTask::run()
{
    // The task participates in the flow only to own its subscription
    // tokens and serve as a subscriber target; there is no per-tick work
    // to perform here.
    return vigine::Result();
}

void ProcessInputEventTask::takeSubscriptionToken(
    std::unique_ptr<vigine::messaging::ISubscriptionToken> token)
{
    if (!token)
        return;

    _tokens.push_back(std::move(token));
}

vigine::messaging::DispatchResult
    ProcessInputEventTask::onMessage(const vigine::messaging::IMessage &message)
{
    const vigine::payload::PayloadTypeId id = message.payloadTypeId();

    if (id == kMouseButtonDownPayloadTypeId)
    {
        if (const auto *payload =
                dynamic_cast<const MouseButtonDownPayload *>(message.payload()))
        {
            onMouseButtonDown(*payload);
            return vigine::messaging::DispatchResult::Handled;
        }
        return vigine::messaging::DispatchResult::Pass;
    }

    if (id == kKeyDownPayloadTypeId)
    {
        if (const auto *payload =
                dynamic_cast<const KeyDownPayload *>(message.payload()))
        {
            onKeyDown(*payload);
            return vigine::messaging::DispatchResult::Handled;
        }
        return vigine::messaging::DispatchResult::Pass;
    }

    return vigine::messaging::DispatchResult::Pass;
}

void ProcessInputEventTask::onMouseButtonDown(const MouseButtonDownPayload &payload)
{
    _hasMouseEvent = true;

    std::cout << "[ProcessInputEventTask::onMouseButtonDown] Mouse button down: "
              << static_cast<int>(payload.button()) << " at " << payload.x() << ", "
              << payload.y() << std::endl;
}

void ProcessInputEventTask::onKeyDown(const KeyDownPayload &payload)
{
    _hasKeyEvent = true;

    const vigine::ecs::platform::KeyEvent &event = payload.event();

    std::cout << "[ProcessInputEventTask::onKeyDown] Key down: code=" << event.keyCode
              << ", scan=" << event.scanCode << ", mods=" << event.modifiers
              << ", repeatCount=" << event.repeatCount
              << ", isRepeat=" << event.isRepeat << std::endl;
}

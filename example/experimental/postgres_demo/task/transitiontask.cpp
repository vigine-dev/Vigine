#include "transitiontask.h"

#include <vigine/api/engine/iengine.h>
#include <vigine/api/statemachine/istatemachine.h>
#include <vigine/result.h>

TransitionTask::TransitionTask(vigine::statemachine::IStateMachine *stateMachine,
                               vigine::statemachine::StateId        target)
    : _stateMachine(stateMachine), _target(target)
{
}

vigine::Result TransitionTask::run()
{
    if (_stateMachine == nullptr)
        return vigine::Result(vigine::Result::Code::Error,
                              "TransitionTask::run: state machine is not bound");

    if (!_target.valid())
        return vigine::Result(vigine::Result::Code::Error,
                              "TransitionTask::run: target state id is invalid");

    // requestTransition is the documented any-thread entry point for
    // queueing a transition; the engine's per-tick drain
    // (processQueuedTransitions) applies it on the controller thread.
    _stateMachine->requestTransition(_target);
    return vigine::Result();
}

ShutdownTask::ShutdownTask(vigine::engine::IEngine *engine) : _engine(engine) {}

vigine::Result ShutdownTask::run()
{
    if (_engine == nullptr)
        return vigine::Result(vigine::Result::Code::Error,
                              "ShutdownTask::run: engine is not bound");

    _engine->shutdown();
    return vigine::Result();
}

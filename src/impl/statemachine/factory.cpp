#include "vigine/api/statemachine/factory.h"

#include <memory>

#include "statemachine/defaultstatemachine.h"

namespace vigine::statemachine
{

std::unique_ptr<IStateMachine> createStateMachine()
{
    // The factory constructs the default concrete closer over
    // AbstractStateMachine. The internal state topology is allocated
    // eagerly by the base class constructor, which also auto-provisions
    // the default state per UD-3, so the returned machine is immediately
    // ready to answer @ref IStateMachine::current with a valid id.
    return std::make_unique<DefaultStateMachine>();
}

} // namespace vigine::statemachine

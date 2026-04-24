#pragma once

#include <memory>

#include "vigine/statemachine/istatemachine.h"

namespace vigine::statemachine
{
/**
 * @brief Constructs the default concrete state machine and hands back
 *        an owning @c std::unique_ptr<IStateMachine>.
 *
 * The factory is the single public entry point callers use to
 * instantiate a state machine in this leaf. The returned object is
 * the minimal concrete closer over @ref AbstractStateMachine; it
 * carries no domain-specific behaviour of its own and exists so the
 * wrapper primitive can be exercised, linked, and tested in
 * isolation.
 *
 * Ownership: the caller owns the returned pointer. Callers that need
 * shared ownership wrap the result in a @c std::shared_ptr at the
 * call site; shared ownership is not the factory's concern. This
 * mirrors the shape used by the ECS factory
 * (@ref vigine::ecs::createECS), the service factory
 * (@ref vigine::service::createService), the thread manager factory
 * (@ref vigine::core::threading::createThreadManager), the payload registry
 * factory, and the message bus factory
 * (@ref vigine::messaging::createMessageBus).
 *
 * Lifetime: the returned machine is self-contained. The internal
 * state topology is constructed eagerly during the factory call and
 * the default state is auto-provisioned per UD-3, so the returned
 * machine has a valid @ref IStateMachine::current id immediately
 * without any further caller action.
 *
 * The function is @c [[nodiscard]] because silently dropping the
 * returned handle would leak the allocation and leave the caller
 * with nothing — the motivation for the @ref FF-1 factory rule.
 */
[[nodiscard]] std::unique_ptr<IStateMachine> createStateMachine();

} // namespace vigine::statemachine

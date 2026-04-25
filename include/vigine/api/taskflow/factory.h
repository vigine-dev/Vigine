#pragma once

#include <memory>

#include "vigine/api/taskflow/itaskflow.h"

namespace vigine::taskflow
{
/**
 * @brief Constructs the default concrete task flow and hands back an
 *        owning @c std::unique_ptr<ITaskFlow>.
 *
 * The factory is the single public entry point callers use to
 * instantiate a task flow in this leaf. The returned object is the
 * minimal concrete closer over @ref AbstractTaskFlow; it carries no
 * domain-specific behaviour of its own and exists so the wrapper
 * primitive can be exercised, linked, and tested in isolation.
 *
 * Ownership: the caller owns the returned pointer. Callers that need
 * shared ownership wrap the result in a @c std::shared_ptr at the
 * call site; shared ownership is not the factory's concern. This
 * mirrors the shape used by the ECS factory
 * (@ref vigine::ecs::createECS), the service factory
 * (@ref vigine::service::createService), the state machine factory
 * (@ref vigine::statemachine::createStateMachine), the thread manager
 * factory (@ref vigine::core::threading::createThreadManager), the payload
 * registry factory, and the message bus factory
 * (@ref vigine::messaging::createMessageBus).
 *
 * Lifetime: the returned flow is self-contained. The internal task
 * orchestrator is constructed eagerly during the factory call and
 * the default task is auto-provisioned per UD-3, so the returned
 * flow has a valid @ref ITaskFlow::current id immediately without
 * any further caller action.
 *
 * The function is @c [[nodiscard]] because silently dropping the
 * returned handle would leak the allocation and leave the caller
 * with nothing — the motivation for the @ref FF-1 factory rule.
 */
[[nodiscard]] std::unique_ptr<ITaskFlow> createTaskFlow();

} // namespace vigine::taskflow

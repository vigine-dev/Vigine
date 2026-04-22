#pragma once

#include "vigine/taskflow/abstracttaskflow.h"

namespace vigine::taskflow
{
/**
 * @brief Minimal concrete task flow that seals the wrapper recipe.
 *
 * @ref DefaultTaskFlow exists so @ref createTaskFlow can return a real
 * owning @c std::unique_ptr<ITaskFlow>. It carries no domain-specific
 * behaviour; every @ref ITaskFlow method falls through to
 * @ref AbstractTaskFlow, which in turn delegates to the internal task
 * orchestrator and applies the UD-3 default-task and FirstMatch-routing
 * behaviours.
 *
 * The class is @c final to close the inheritance chain for this leaf;
 * follow-up leaves that specialise the task flow with their own caches
 * or indices define their own concrete classes and their own factory
 * entry points and do not derive from @ref DefaultTaskFlow.
 */
class DefaultTaskFlow final : public AbstractTaskFlow
{
  public:
    DefaultTaskFlow();
    ~DefaultTaskFlow() override;

    DefaultTaskFlow(const DefaultTaskFlow &)            = delete;
    DefaultTaskFlow &operator=(const DefaultTaskFlow &) = delete;
    DefaultTaskFlow(DefaultTaskFlow &&)                 = delete;
    DefaultTaskFlow &operator=(DefaultTaskFlow &&)      = delete;
};

} // namespace vigine::taskflow

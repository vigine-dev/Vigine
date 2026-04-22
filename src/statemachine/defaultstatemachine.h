#pragma once

#include "vigine/statemachine/abstractstatemachine.h"

namespace vigine::statemachine
{
/**
 * @brief Minimal concrete state machine that seals the wrapper recipe.
 *
 * @ref DefaultStateMachine exists so @ref createStateMachine can
 * return a real owning @c std::unique_ptr<IStateMachine>. It carries
 * no domain-specific behaviour; every @ref IStateMachine method
 * falls through to @ref AbstractStateMachine, which in turn
 * delegates to the internal state topology and applies the UD-3
 * default-state and bubble-routing behaviours.
 *
 * The class is @c final to close the inheritance chain for this
 * leaf; follow-up leaves that specialise the state machine with
 * their own caches or indices define their own concrete classes and
 * their own factory entry points and do not derive from
 * @ref DefaultStateMachine.
 */
class DefaultStateMachine final : public AbstractStateMachine
{
  public:
    DefaultStateMachine();
    ~DefaultStateMachine() override;

    DefaultStateMachine(const DefaultStateMachine &)            = delete;
    DefaultStateMachine &operator=(const DefaultStateMachine &) = delete;
    DefaultStateMachine(DefaultStateMachine &&)                 = delete;
    DefaultStateMachine &operator=(DefaultStateMachine &&)      = delete;
};

} // namespace vigine::statemachine

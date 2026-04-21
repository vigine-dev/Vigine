#pragma once

namespace vigine
{
/**
 * @brief Pure-virtual forward-declared stub for the state-machine surface.
 *
 * @ref IStateMachine is a minimal stub whose only contract is a virtual
 * destructor. It exists so that @ref Engine::state can return a
 * reference to a pure-virtual interface without requiring the
 * state-machine domain to be finalised in this leaf. The finalised
 * surface -- transitions, state lookup, tick semantics, and so on --
 * lands in a later leaf that refactors @c StateMachine onto this
 * interface.
 *
 * Ownership: this type is never instantiated directly. Concrete
 * @c StateMachine objects derive from it and are owned by the
 * @ref Engine as @c std::unique_ptr.
 */
class IStateMachine
{
  public:
    virtual ~IStateMachine() = default;

    IStateMachine(const IStateMachine &)            = delete;
    IStateMachine &operator=(const IStateMachine &) = delete;
    IStateMachine(IStateMachine &&)                 = delete;
    IStateMachine &operator=(IStateMachine &&)      = delete;

  protected:
    IStateMachine() = default;
};

} // namespace vigine

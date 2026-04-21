#pragma once

namespace vigine
{
/**
 * @brief Pure-virtual forward-declared stub for the engine context.
 *
 * @ref IContext is a minimal stub whose only contract is a virtual
 * destructor. It exists so that @ref Engine::context can return a
 * reference to a pure-virtual interface without requiring the context
 * domain (service container, system registry, binding host) to be
 * finalised in this leaf. The finalised surface lands in a later leaf
 * that refactors @c Context onto this interface.
 *
 * Ownership: the stub is never instantiated directly. Concrete
 * @c Context objects derive from it and are owned by the @ref Engine as
 * @c std::unique_ptr.
 */
class IContext
{
  public:
    virtual ~IContext() = default;

    IContext(const IContext &)            = delete;
    IContext &operator=(const IContext &) = delete;
    IContext(IContext &&)                 = delete;
    IContext &operator=(IContext &&)      = delete;

  protected:
    IContext() = default;
};

} // namespace vigine

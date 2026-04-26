#pragma once

#include "vigine/api/context/abstractcontext.h"
#include "vigine/api/context/contextconfig.h"

namespace vigine::context
{
/**
 * @brief Minimal concrete aggregator that seals the wrapper recipe.
 *
 * @ref Context exists so @ref createContext can return a real
 * owning @c std::unique_ptr<IContext>. It carries no domain-specific
 * behaviour; its accessors fall through to @ref AbstractContext. The
 * class is @c final to close the inheritance chain for this leaf;
 * follow-up leaves that ship specialised contexts (e.g. test fixtures,
 * embedded harnesses) derive from @ref AbstractContext directly and
 * supply their own factory entry points.
 */
class Context final : public AbstractContext
{
  public:
    explicit Context(const ContextConfig &config);
    ~Context() override;

    Context(const Context &)            = delete;
    Context &operator=(const Context &) = delete;
    Context(Context &&)                 = delete;
    Context &operator=(Context &&)      = delete;
};

} // namespace vigine::context

#pragma once

#include "vigine/context/abstractcontext.h"
#include "vigine/context/contextconfig.h"

namespace vigine::context
{
/**
 * @brief Minimal concrete aggregator that seals the wrapper recipe.
 *
 * @ref DefaultContext exists so @ref createContext can return a real
 * owning @c std::unique_ptr<IContext>. It carries no domain-specific
 * behaviour; its accessors fall through to @ref AbstractContext. The
 * class is @c final to close the inheritance chain for this leaf;
 * follow-up leaves that ship specialised contexts (e.g. test fixtures,
 * embedded harnesses) derive from @ref AbstractContext directly and
 * supply their own factory entry points.
 */
class DefaultContext final : public AbstractContext
{
  public:
    explicit DefaultContext(const ContextConfig &config);
    ~DefaultContext() override;

    DefaultContext(const DefaultContext &)            = delete;
    DefaultContext &operator=(const DefaultContext &) = delete;
    DefaultContext(DefaultContext &&)                 = delete;
    DefaultContext &operator=(DefaultContext &&)      = delete;
};

} // namespace vigine::context

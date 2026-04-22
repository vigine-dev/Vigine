#pragma once

#include "vigine/engine/abstractengine.h"
#include "vigine/engine/engineconfig.h"

namespace vigine::engine
{
/**
 * @brief Minimal concrete engine that seals the wrapper recipe.
 *
 * @ref DefaultEngine exists so @ref createEngine can return a real
 * owning @c std::unique_ptr<IEngine>. It carries no domain-specific
 * behaviour; its accessors fall through to @ref AbstractEngine. The
 * class is @c final to close the inheritance chain for this leaf;
 * follow-up leaves that ship specialised engines (e.g. test fixtures,
 * embedded harnesses) derive from @ref AbstractEngine directly and
 * supply their own factory entry points.
 */
class DefaultEngine final : public AbstractEngine
{
  public:
    explicit DefaultEngine(const EngineConfig &config);
    ~DefaultEngine() override;

    DefaultEngine(const DefaultEngine &)            = delete;
    DefaultEngine &operator=(const DefaultEngine &) = delete;
    DefaultEngine(DefaultEngine &&)                 = delete;
    DefaultEngine &operator=(DefaultEngine &&)      = delete;
};

} // namespace vigine::engine

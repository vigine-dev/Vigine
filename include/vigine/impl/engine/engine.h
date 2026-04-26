#pragma once

#include "vigine/api/engine/abstractengine.h"
#include "vigine/api/engine/engineconfig.h"

namespace vigine::engine
{
/**
 * @brief Minimal concrete engine that seals the wrapper recipe.
 *
 * @ref Engine exists so @ref createEngine can return a real
 * owning @c std::unique_ptr<IEngine>. It carries no domain-specific
 * behaviour; its accessors fall through to @ref AbstractEngine. The
 * class is @c final to close the inheritance chain for this leaf;
 * follow-up leaves that ship specialised engines (e.g. test fixtures,
 * embedded harnesses) derive from @ref AbstractEngine directly and
 * supply their own factory entry points.
 */
class Engine final : public AbstractEngine
{
  public:
    explicit Engine(const EngineConfig &config);
    ~Engine() override;

    Engine(const Engine &)            = delete;
    Engine &operator=(const Engine &) = delete;
    Engine(Engine &&)                 = delete;
    Engine &operator=(Engine &&)      = delete;
};

} // namespace vigine::engine

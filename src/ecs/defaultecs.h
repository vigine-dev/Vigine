#pragma once

#include "vigine/api/ecs/abstractecs.h"

namespace vigine::ecs
{
/**
 * @brief Minimal concrete ECS that seals the wrapper recipe.
 *
 * @ref DefaultECS exists so @ref createECS can return a real owning
 * @c std::unique_ptr<IECS>. It carries no domain-specific behaviour;
 * every @ref IECS method falls through to @ref AbstractECS, which in
 * turn delegates to the internal entity world.
 *
 * The class is @c final to close the inheritance chain for this leaf;
 * follow-up leaves that specialise the ECS with their own caches or
 * indices define their own concrete classes and their own factory
 * entry points and do not derive from @ref DefaultECS.
 */
class DefaultECS final : public AbstractECS
{
  public:
    DefaultECS();
    ~DefaultECS() override;

    DefaultECS(const DefaultECS &)            = delete;
    DefaultECS &operator=(const DefaultECS &) = delete;
    DefaultECS(DefaultECS &&)                 = delete;
    DefaultECS &operator=(DefaultECS &&)      = delete;
};

} // namespace vigine::ecs

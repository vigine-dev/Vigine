#pragma once

#include "vigine/api/ecs/ientitymanager.h"

namespace vigine
{
/**
 * @brief Stateful abstract base between @ref IEntityManager and the
 *        concrete @ref EntityManager.
 *
 * @ref AbstractEntityManager is the level-4 base in the wrapper recipe
 * applied to the legacy entity manager: it provides default
 * implementations of every @ref IEntityManager method the concrete
 * needs and sequences construction so a derived closer can plug in
 * without re-implementing the ownership story.
 *
 * The base carries no state of its own at this leaf; the legacy
 * @ref EntityManager keeps its private @c std::vector and @c std::map
 * because re-homing them would expand this leaf beyond the agreed
 * scope. Follow-up work folds those substrate fields here so the
 * concrete becomes a minimal closer, mirroring how
 * @ref AbstractContext / @c Context split.
 *
 * Strict encapsulation: the class follows the project rule that all
 * data members are @c private; derived closers reach base state through
 * @c protected accessors. Today no data lives here, so no accessor is
 * exposed; the door is open for the follow-up leaf to add them without
 * a source-compat break.
 *
 * INV-1 compliance: no template parameters on the public surface.
 */
class AbstractEntityManager : public IEntityManager
{
  public:
    ~AbstractEntityManager() override = default;

    AbstractEntityManager(const AbstractEntityManager &)            = delete;
    AbstractEntityManager &operator=(const AbstractEntityManager &) = delete;
    AbstractEntityManager(AbstractEntityManager &&)                 = delete;
    AbstractEntityManager &operator=(AbstractEntityManager &&)      = delete;

  protected:
    AbstractEntityManager() = default;
};

} // namespace vigine

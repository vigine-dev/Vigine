#pragma once

#include <string>

namespace vigine::ecs
{
/**
 * @brief Pure-virtual root interface for legacy ECS systems.
 *
 * @ref ISystem captures the minimal stable surface every legacy system
 * exposes: a stable string @ref id naming the system family (e.g.
 * @c "Render", @c "Window") and the virtual destructor required for
 * owning containers.
 *
 * The legacy abstract base @c vigine::AbstractSystem (declared in
 * @c include/vigine/api/ecs/abstractsystem.h) derives from this
 * interface so existing concrete systems satisfy the contract without
 * source changes. The @c vigine::ecs:: namespace placement matches the
 * forward declaration on @c IEngineToken (see
 * @c include/vigine/api/engine/iengine_token.h).
 *
 * INV-1 compliance: no template parameters anywhere on the surface.
 * INV-10 compliance: the @c I prefix marks a pure-virtual interface
 * with no state and no non-virtual method bodies. The minimal shape
 * keeps the door open for follow-up leaves (#280, #287) that finalise
 * the system surface without needing to revise an over-specified
 * contract here.
 */
class ISystem
{
  public:
    /**
     * @brief Stable system-family identifier.
     *
     * The legacy implementation uses a @c std::string; concrete
     * systems expose a constant they own (e.g. @c "Render"). The
     * follow-up leaf that introduces the system manager keys lookups
     * on this value, so subclasses must keep the returned string
     * stable for the system's lifetime.
     */
    using Id = std::string;

    virtual ~ISystem() = default;

    /**
     * @brief Returns the stable system-family identifier.
     */
    [[nodiscard]] virtual Id id() const = 0;

    ISystem(const ISystem &)            = delete;
    ISystem &operator=(const ISystem &) = delete;
    ISystem(ISystem &&)                 = delete;
    ISystem &operator=(ISystem &&)      = delete;

  protected:
    ISystem() = default;
};

} // namespace vigine::ecs

#pragma once

/**
 * @file componentmanager.h
 * @brief Legacy concrete store for ECS components, keyed by ComponentKind.
 */

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "vigine/api/ecs/abstractcomponentmanager.h"
#include "vigine/api/ecs/componentkind.h"
#include "vigine/api/ecs/icomponent.h"
#include "vigine/result.h"

namespace vigine
{
using ComponentUPtr = std::unique_ptr<IComponent>;

/**
 * @brief Concrete component store keyed by @ref ComponentKind.
 *
 * Closes the legacy component-manager chain: derives from
 * @ref AbstractComponentManager, which in turn implements the pure
 * @ref IComponentManager contract. Replaces the template-based store
 * the legacy version carried before this leaf with virtual-dispatch
 * keyed on @ref ComponentKind so the public API exports no template
 * parameters (INV-1).
 *
 * Storage shape:
 *   - @c std::unordered_map<ComponentKind, std::vector<ComponentUPtr>>
 *     -- one bucket per registered kind; insertion order preserved
 *     within the bucket so callers iterating @ref components observe
 *     a deterministic sequence.
 *
 * Ownership: every component is held by @c std::unique_ptr; the
 * manager outlives every component it stores. Callers that need
 * shared ownership keep their own handle and pass a clone via
 * @ref add at registration time.
 */
class ComponentManager final : public AbstractComponentManager
{
  public:
    ComponentManager();
    ~ComponentManager() override;

    [[nodiscard]] Result add(ComponentUPtr component) override;

    [[nodiscard]] std::vector<const IComponent *>
        components(ComponentKind kind) const override;

    std::size_t removeAllOfKind(ComponentKind kind) noexcept override;

    void clear() noexcept override;

    ComponentManager(const ComponentManager &)            = delete;
    ComponentManager &operator=(const ComponentManager &) = delete;
    ComponentManager(ComponentManager &&)                 = delete;
    ComponentManager &operator=(ComponentManager &&)      = delete;

  private:
    /**
     * @brief Hash adapter for @ref ComponentKind so it can key an
     *        @c unordered_map without dragging in a global
     *        @c std::hash specialisation.
     */
    struct KindHash
    {
        std::size_t operator()(ComponentKind kind) const noexcept
        {
            return std::hash<std::uint32_t>{}(static_cast<std::uint32_t>(kind));
        }
    };

    std::unordered_map<ComponentKind, std::vector<ComponentUPtr>, KindHash> _components;
};

} // namespace vigine

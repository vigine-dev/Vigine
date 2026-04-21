#pragma once

#include <compare>
#include <cstdint>

namespace vigine::ecs
{
/**
 * @brief Generational identifier for an entity managed by an @ref IECS.
 *
 * @ref EntityId is a POD value type owned by the ECS wrapper. It is
 * deliberately its own struct rather than an alias over the substrate
 * primitive's identifier type so that the public ECS surface never
 * mentions substrate primitive types (INV-11 — wrapper encapsulation).
 * The wrapper layer translates between @c EntityId and the substrate-
 * side identifiers exclusively inside @c src/ecs; callers of the
 * @ref IECS API never need to know the substrate exists.
 *
 * The @c index field addresses a slot in the internal entity world; the
 * @c generation field is incremented whenever that slot is recycled so a
 * lookup with a stale handle fails safely rather than returning a
 * different entity. A default-constructed @ref EntityId
 * (@c generation @c == @c 0) is the invalid sentinel and reports
 * @ref valid as @c false.
 *
 * The pair is small (8 bytes), trivially copyable, and safe to pass by
 * value across thread boundaries.
 *
 * @note The retroactive fix from user decision UD-8 replaces the
 *       earlier typedef alias with this own POD. The layout is
 *       structurally identical but the named type keeps substrate
 *       types out of the public header tree.
 */
struct EntityId
{
    std::uint32_t index{0};
    std::uint32_t generation{0};

    /**
     * @brief Reports whether the id addresses a slot that was live at
     *        construction time.
     *
     * A @c true return only means the generation is non-zero. The
     * underlying entity world may still have invalidated the slot since;
     * the authoritative check is an @ref IECS lookup.
     */
    [[nodiscard]] constexpr bool valid() const noexcept { return generation != 0; }

    friend constexpr auto operator<=>(const EntityId &, const EntityId &) = default;
    friend constexpr bool operator==(const EntityId &, const EntityId &)  = default;
};

/**
 * @brief Generational handle for an individual component attached to an
 *        entity.
 *
 * @ref ComponentHandle is the second own POD owned by the ECS wrapper.
 * It identifies a specific component-attachment slot — the combination
 * of an entity and a component type stored under the entity's
 * attachment list — without exposing substrate primitive identifier
 * types. A default-constructed handle is the invalid sentinel.
 *
 * The handle stays stable across lookups while the component remains
 * attached; detaching the component bumps the slot generation so stale
 * handles fail safely.
 */
struct ComponentHandle
{
    std::uint32_t index{0};
    std::uint32_t generation{0};

    /**
     * @brief Reports whether the handle referenced a live component at
     *        construction time.
     */
    [[nodiscard]] constexpr bool valid() const noexcept { return generation != 0; }

    friend constexpr auto operator<=>(const ComponentHandle &, const ComponentHandle &) = default;
    friend constexpr bool operator==(const ComponentHandle &, const ComponentHandle &)  = default;
};

/**
 * @brief Opaque tag used to classify a component type.
 *
 * Concrete component implementations report a stable value from
 * @ref IComponent::componentTypeId. Reserved ranges mirror the
 * @c PayloadTypeId discipline used by the messaging wrapper:
 *   - @c [0..127] — engine-bundled component types.
 *   - @c [128..) — user-space component types.
 *
 * @ref IECS treats the id as opaque; concrete component receivers
 * assert on the expected value and @c static_cast down to the concrete
 * type. No @c dynamic_cast and no @c std::any on the subscriber path
 * (INV-1).
 */
using ComponentTypeId = std::uint32_t;

} // namespace vigine::ecs

#pragma once

#include <compare>
#include <cstdint>

namespace vigine::statemachine
{
/**
 * @brief Generational identifier for a state managed by an
 *        @ref IStateMachine.
 *
 * @ref StateId is a POD value type owned by the state machine wrapper.
 * It is deliberately its own struct rather than an alias over the
 * substrate primitive's identifier type so the public state machine
 * surface never mentions substrate primitive types (INV-11 — wrapper
 * encapsulation). The wrapper layer translates between @c StateId and
 * the substrate-side identifiers exclusively inside
 * @c src/statemachine; callers of the @ref IStateMachine API never
 * need to know the substrate exists.
 *
 * The @c index field addresses a slot in the internal state topology;
 * the @c generation field is incremented whenever that slot is recycled
 * so a lookup with a stale handle fails safely rather than returning a
 * different state. A default-constructed @ref StateId (@c generation
 * @c == @c 0) is the invalid sentinel and reports @ref valid as
 * @c false.
 *
 * The pair is small (8 bytes), trivially copyable, and safe to pass by
 * value across thread boundaries.
 *
 * @note The layout is intentionally structurally identical to the
 *       substrate's own generational identifier. The translation
 *       lives exclusively inside the wrapper implementation —
 *       keeping substrate types out of the public header tree is
 *       the whole point of the separate type.
 */
// ENCAP EXEMPT: pure value aggregate
struct StateId
{
    std::uint32_t index{0};
    std::uint32_t generation{0};

    /**
     * @brief Reports whether the id addresses a slot that was live at
     *        construction time.
     *
     * A @c true return only means the generation is non-zero. The
     * underlying state topology may still have invalidated the slot
     * since; the authoritative check is an @ref IStateMachine lookup.
     */
    [[nodiscard]] constexpr bool valid() const noexcept { return generation != 0; }

    friend constexpr auto operator<=>(const StateId &, const StateId &) = default;
    friend constexpr bool operator==(const StateId &, const StateId &)  = default;
};

} // namespace vigine::statemachine

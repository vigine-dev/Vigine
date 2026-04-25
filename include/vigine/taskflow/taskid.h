#pragma once

#include <compare>
#include <cstdint>

namespace vigine::taskflow
{
/**
 * @brief Generational identifier for a task managed by an
 *        @ref ITaskFlow.
 *
 * @ref TaskId is a POD value type owned by the task flow wrapper. It is
 * deliberately its own struct rather than an alias over the substrate
 * primitive's identifier type so the public task flow surface never
 * mentions substrate primitive types (INV-11 — wrapper encapsulation).
 * The wrapper layer translates between @c TaskId and the substrate-side
 * identifiers exclusively inside @c src/taskflow; callers of the
 * @ref ITaskFlow API never need to know the substrate exists.
 *
 * The @c index field addresses a slot in the internal task orchestrator;
 * the @c generation field is incremented whenever that slot is recycled
 * so a lookup with a stale handle fails safely rather than returning a
 * different task. A default-constructed @ref TaskId (@c generation
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
struct TaskId
{
    std::uint32_t index{0};
    std::uint32_t generation{0};

    /**
     * @brief Reports whether the id addresses a slot that was live at
     *        construction time.
     *
     * A @c true return only means the generation is non-zero. The
     * underlying task orchestrator may still have invalidated the slot
     * since; the authoritative check is an @ref ITaskFlow lookup.
     */
    [[nodiscard]] constexpr bool valid() const noexcept { return generation != 0; }

    friend constexpr auto operator<=>(const TaskId &, const TaskId &) = default;
    friend constexpr bool operator==(const TaskId &, const TaskId &)  = default;
};

} // namespace vigine::taskflow

#pragma once

#include <compare>
#include <cstdint>

namespace vigine::graph
{
/**
 * @brief Generational identifier of a graph edge.
 *
 * POD value type with the same layout contract as @ref NodeId. Generation
 * `0` is the invalid sentinel; lookups with a stale identifier fail safely.
 */
struct EdgeId
{
    std::uint32_t index{0};
    std::uint32_t generation{0};

    /**
     * @brief Reports whether the identifier refers to a live slot at
     *        construction time.
     */
    [[nodiscard]] constexpr bool valid() const noexcept { return generation != 0; }

    friend constexpr auto operator<=>(const EdgeId &, const EdgeId &) = default;
    friend constexpr bool operator==(const EdgeId &, const EdgeId &)  = default;
};

} // namespace vigine::graph

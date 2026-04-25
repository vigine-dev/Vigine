#pragma once

#include <compare>
#include <cstdint>

namespace vigine::core::graph
{
/**
 * @brief Generational identifier of a graph node.
 *
 * POD value type carried by every client that refers to a node owned by an
 * @ref IGraph. Identifiers are composed of a slot index and a generation
 * counter. The counter bumps each time a slot is reused, so a lookup with a
 * stale identifier fails safely rather than returning a different node.
 *
 * @note Generation `0` is reserved as the invalid sentinel. A default
 *       constructed @ref NodeId is therefore always invalid and never
 *       returned by @ref IGraph::addNode.
 */
// ENCAP EXEMPT: pure value aggregate
struct NodeId
{
    std::uint32_t index{0};
    std::uint32_t generation{0};

    /**
     * @brief Reports whether the identifier refers to a live slot at
     *        construction time.
     *
     * A `true` return only means the generation is non-zero. The graph may
     * still have invalidated the slot since; use @ref IGraph::node or
     * @ref IGraphQuery::hasNode for the authoritative check.
     */
    [[nodiscard]] constexpr bool valid() const noexcept { return generation != 0; }

    friend constexpr auto operator<=>(const NodeId &, const NodeId &) = default;
    friend constexpr bool operator==(const NodeId &, const NodeId &)  = default;
};

} // namespace vigine::core::graph

#pragma once

#include <cstdint>

namespace vigine::messaging
{
/**
 * @brief Generational identifier for a single bus-side connection slot.
 *
 * @ref ConnectionId is a POD value type. The @c index field addresses a
 * slot in an @ref IBusControlBlock registry; the @c generation field is
 * incremented whenever that slot is recycled. A connection id whose
 * @c generation is zero is the default-constructed sentinel and reports
 * @ref valid as @c false so that call sites can detect uninitialised
 * handles without reaching into the slot table.
 *
 * The pair is small (8 bytes), trivially copyable, and safe to pass by
 * value across thread boundaries. Control-block implementations issue
 * ids starting at @c generation == 1; zero is reserved for the sentinel.
 */
struct ConnectionId
{
    std::uint32_t index{0};
    std::uint32_t generation{0};

    /**
     * @brief Returns @c true when the id addresses a live slot.
     *
     * The sentinel (@c generation == 0) is not valid. A non-zero
     * generation is accepted as valid even when the matching slot has
     * since been recycled; stale-slot detection is the control block's
     * responsibility and happens at lookup time.
     */
    [[nodiscard]] constexpr bool valid() const noexcept { return generation != 0; }

    [[nodiscard]] friend constexpr bool operator==(ConnectionId lhs,
                                                   ConnectionId rhs) noexcept
    {
        return lhs.index == rhs.index && lhs.generation == rhs.generation;
    }

    [[nodiscard]] friend constexpr bool operator!=(ConnectionId lhs,
                                                   ConnectionId rhs) noexcept
    {
        return !(lhs == rhs);
    }
};

} // namespace vigine::messaging

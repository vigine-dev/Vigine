#pragma once

#include <cstdint>

namespace vigine::actorhost
{

/**
 * @brief Generational actor identity.
 *
 * @ref ActorId is a lightweight POD value that uniquely identifies a live
 * actor inside an @ref IActorHost.  The @c value field carries a generational
 * counter so that a freshly spawned actor never reuses the numeric identity
 * of a previously stopped one.
 *
 * Sentinel: @c value == 0 means "invalid / no actor".  Live ids always start
 * at @c 1.
 *
 * Invariants:
 *   - INV-10: POD value type; no template parameters (INV-1).
 *   - INV-11: no graph types here.
 */
// ENCAP EXEMPT: pure value aggregate
struct ActorId
{
    std::uint32_t value{0};

    [[nodiscard]] constexpr bool valid() const noexcept { return value != 0; }

    [[nodiscard]] friend constexpr bool operator==(ActorId lhs, ActorId rhs) noexcept
    {
        return lhs.value == rhs.value;
    }

    [[nodiscard]] friend constexpr bool operator!=(ActorId lhs, ActorId rhs) noexcept
    {
        return lhs.value != rhs.value;
    }
};

} // namespace vigine::actorhost

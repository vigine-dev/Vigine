#pragma once

#include <cstdint>

namespace vigine::messaging
{
/**
 * @brief Opaque value identifier for an @ref IMessageBus instance.
 *
 * Every bus carries its own @ref BusId so that facades, engine-internal
 * plumbing, and diagnostics can name a specific bus without relying on
 * raw pointers. The engine assigns @c value @c == @c 1 to the system bus
 * and increments from there for every additional bus created through the
 * factory. A default-constructed @ref BusId (value zero) is the sentinel
 * and is never issued to a live bus; it is returned from error paths so
 * that callers can distinguish "no bus" from a valid handle.
 *
 * The struct is trivially copyable and safe to pass by value across
 * thread boundaries; equality is defined on the underlying value.
 */
struct BusId
{
    std::uint32_t value{0};

    [[nodiscard]] constexpr bool valid() const noexcept { return value != 0; }

    [[nodiscard]] friend constexpr bool operator==(BusId lhs, BusId rhs) noexcept
    {
        return lhs.value == rhs.value;
    }

    [[nodiscard]] friend constexpr bool operator!=(BusId lhs, BusId rhs) noexcept
    {
        return lhs.value != rhs.value;
    }
};

} // namespace vigine::messaging

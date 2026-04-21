#pragma once

#include <cstdint>

namespace vigine::payload
{
/**
 * @brief Opaque identifier for a payload type carried through the engine.
 *
 * @ref PayloadTypeId is a thin value type wrapping a @c std::uint32_t. The
 * wrapper exists so that the public surface never passes a bare integer
 * that could be confused with an entity id, a node id, or any other
 * numeric identifier. The underlying @c value field is accessible for
 * hashing, logging, and static-cast discipline at integration points.
 *
 * Ranges carved out of the 32-bit space are declared in
 * @ref payloadrange.h. Concrete identifiers are issued by the engine and
 * by application code and registered with @ref IPayloadRegistry before
 * first use.
 *
 * The type is a plain aggregate: trivially copyable, comparable, and
 * usable as a key in ordered and hash-based associative containers.
 */
struct PayloadTypeId
{
    std::uint32_t value{0};

    [[nodiscard]] friend constexpr bool operator==(PayloadTypeId lhs,
                                                   PayloadTypeId rhs) noexcept
    {
        return lhs.value == rhs.value;
    }

    [[nodiscard]] friend constexpr bool operator!=(PayloadTypeId lhs,
                                                   PayloadTypeId rhs) noexcept
    {
        return lhs.value != rhs.value;
    }

    [[nodiscard]] friend constexpr bool operator<(PayloadTypeId lhs,
                                                  PayloadTypeId rhs) noexcept
    {
        return lhs.value < rhs.value;
    }
};

} // namespace vigine::payload

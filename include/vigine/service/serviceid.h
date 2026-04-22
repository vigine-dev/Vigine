#pragma once

#include <compare>
#include <cstdint>

namespace vigine::service
{
/**
 * @brief Generational identifier for a service registered on an
 *        @ref IService container.
 *
 * @ref ServiceId is a POD value type owned by the service wrapper. It is
 * deliberately its own struct rather than an alias over the substrate
 * primitive's identifier type so that the public service surface never
 * mentions substrate primitive types (INV-11 — wrapper encapsulation).
 * The wrapper layer translates between @c ServiceId and the substrate-
 * side identifiers exclusively inside @c src/service; callers of the
 * @ref IService API never need to know the substrate exists underneath.
 *
 * The @c index field addresses a slot in the service registry; the
 * @c generation field is incremented whenever that slot is recycled so a
 * lookup with a stale handle fails safely rather than returning a
 * different service instance. A default-constructed @ref ServiceId
 * (@c generation @c == @c 0) is the invalid sentinel and reports
 * @ref valid as @c false.
 *
 * The pair is small (8 bytes), trivially copyable, and safe to pass by
 * value across thread boundaries.
 */
struct ServiceId
{
    std::uint32_t index{0};
    std::uint32_t generation{0};

    /**
     * @brief Reports whether the id addresses a slot that was live at
     *        construction time.
     *
     * A @c true return only means the generation is non-zero. The
     * registry may still have invalidated the slot since; the
     * authoritative check is the registry lookup performed by
     * @ref IService implementations.
     */
    [[nodiscard]] constexpr bool valid() const noexcept { return generation != 0; }

    friend constexpr auto operator<=>(const ServiceId &, const ServiceId &) = default;
    friend constexpr bool operator==(const ServiceId &, const ServiceId &) = default;
};

} // namespace vigine::service

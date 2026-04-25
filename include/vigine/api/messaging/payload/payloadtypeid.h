#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

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
 * usable as a key in ordered and hash-based associative containers. A
 * specialisation of @c std::hash is provided at the bottom of this
 * header so that @c std::unordered_map and @c std::unordered_set accept
 * @ref PayloadTypeId keys without requiring each caller to write their
 * own hasher.
 */
// ENCAP EXEMPT: pure value aggregate
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

// std::hash specialisation — the doc-comment on PayloadTypeId advertises
// hash-container usability; shipping the specialisation here keeps that
// promise. Keyed off the wrapped uint32_t and delegated to the std::hash
// for uint32_t so the quality matches the standard library's default.
// TEMPLATE EXEMPTION: std::hash specialization required for hash-map key support; sanctioned per architecture.md § R-NoTemplates.
// Wrapped in `namespace std { ... }` (rather than the `template <>
// struct std::hash<...>` global-scope form) for portability — the
// global form is accepted by some compilers but the namespace-qualified
// form is the form mandated by the standard for user specialisations
// inside std.
namespace std
{
template <>
struct hash<vigine::payload::PayloadTypeId>
{
    [[nodiscard]] std::size_t
    operator()(vigine::payload::PayloadTypeId id) const noexcept
    {
        return std::hash<std::uint32_t>{}(id.value);
    }
};
} // namespace std

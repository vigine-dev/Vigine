#pragma once

#include <cstdint>
#include <functional>

namespace vigine::topicbus
{

/**
 * @brief POD identifier for a named pub/sub topic.
 *
 * @ref TopicId is a thin value type wrapping a @c std::uint32_t. In v1 the
 * value is derived from a hash of the topic name; collision resolution is
 * internal to @ref AbstractTopicBus.
 *
 * The type is a plain aggregate: trivially copyable, comparable, and usable
 * as a key in ordered and hash-based associative containers.
 *
 * Reused by plan_19 (request facade) to address reply targets.
 *
 * Invariants:
 *   - A zero @c value is the sentinel for "invalid / not found".
 *   - INV-11: no graph types appear in this header.
 */
// ENCAP EXEMPT: pure value aggregate
struct TopicId
{
    std::uint32_t value{0};

    [[nodiscard]] constexpr bool valid() const noexcept { return value != 0; }

    [[nodiscard]] friend constexpr bool operator==(TopicId lhs, TopicId rhs) noexcept
    {
        return lhs.value == rhs.value;
    }

    [[nodiscard]] friend constexpr bool operator!=(TopicId lhs, TopicId rhs) noexcept
    {
        return lhs.value != rhs.value;
    }

    [[nodiscard]] friend constexpr bool operator<(TopicId lhs, TopicId rhs) noexcept
    {
        return lhs.value < rhs.value;
    }
};

} // namespace vigine::topicbus

// TEMPLATE EXEMPTION: std::hash specialization required for hash-map key support; sanctioned per architecture.md § R-NoTemplates.
template <>
struct std::hash<vigine::topicbus::TopicId>
{
    [[nodiscard]] std::size_t operator()(vigine::topicbus::TopicId id) const noexcept
    {
        return std::hash<std::uint32_t>{}(id.value);
    }
};

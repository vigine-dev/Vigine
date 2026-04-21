#pragma once

#include <compare>
#include <cstdint>

namespace vigine::threading
{
/**
 * @brief Generational identifier of a named thread registered with
 *        @ref IThreadManager::registerNamedThread.
 *
 * POD value type. Callers receive one from
 * @ref IThreadManager::registerNamedThread and pass it back into
 * @ref IThreadManager::scheduleOnNamed. The generation counter bumps each
 * time a slot is recycled so a stale id never resolves to a different
 * thread after a prior @ref IThreadManager::unregisterNamedThread.
 *
 * @note Generation `0` is reserved as the invalid sentinel. A
 *       default-constructed @ref NamedThreadId is therefore always invalid
 *       and never returned from a successful registration.
 */
struct NamedThreadId
{
    std::uint32_t index{0};
    std::uint32_t generation{0};

    /**
     * @brief Reports whether the identifier carries a non-zero generation.
     *
     * A @c true return only means the generation is non-zero. The thread
     * manager may still have unregistered the slot since; callers that
     * need the authoritative answer should attempt a schedule or query
     * the thread manager's @ref IThreadManager::namedThreadCount.
     */
    [[nodiscard]] constexpr bool valid() const noexcept { return generation != 0; }

    friend constexpr auto operator<=>(const NamedThreadId &, const NamedThreadId &) = default;
    friend constexpr bool operator==(const NamedThreadId &, const NamedThreadId &)  = default;
};

} // namespace vigine::threading

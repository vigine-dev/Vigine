#pragma once

#include <cstdint>

namespace vigine::threading
{
/**
 * @brief Closed enumeration of scheduling targets accepted by @ref IThreadManager.
 *
 * A caller never picks a thread directly. It picks an @ref ThreadAffinity
 * value; the thread manager decides which physical OS thread will actually
 * pull the runnable off its queue. The set is deliberately closed — new
 * affinities require an architectural decision, not a caller opt-in.
 *
 * Analogy: the thread manager is a restaurant pass. @ref ThreadAffinity
 * names the station a ticket (runnable) belongs on. One station has many
 * cooks (@c Pool), one has a single reserved cook (@c Dedicated), one is
 * the host station on the main floor (@c Main), and one is a station that
 * answers to a specific name (@c Named). @c Any lets the pass pick the
 * fastest free station.
 */
enum class ThreadAffinity : std::uint8_t
{
    /// Engine picks whichever available worker is fastest to dispatch on.
    Any = 0,
    /// Strictly the main thread. Drains via @ref IThreadManager::runMainThreadPump.
    Main = 1,
    /// One-per-caller dedicated thread with FIFO queue. Lazy-allocated.
    Dedicated = 2,
    /// Worker pool. Parallel execution across the configured pool size.
    Pool = 3,
    /// Explicit named thread identified by @ref NamedThreadId.
    Named = 4,
};

} // namespace vigine::threading

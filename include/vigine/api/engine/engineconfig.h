#pragma once

#include <cstdint>

#include "vigine/api/context/contextconfig.h"

namespace vigine::engine
{
/**
 * @brief Closed enumeration of engine run modes.
 *
 * The run mode tells @ref IEngine::run how long to hold the calling
 * thread and which kind of pump work to schedule while it holds it.
 *
 *   - @c Foreground  -- the canonical production mode. @ref IEngine::run
 *                       blocks the calling thread (expected to be the
 *                       OS main thread) on the thread manager's main
 *                       pump until @ref IEngine::shutdown returns. Every
 *                       post-back submitted through
 *                       @ref core::threading::IThreadManager::postToMainThread
 *                       is drained by the pump tick.
 *   - @c Background  -- same pump semantics as @c Foreground, but the
 *                       caller is expected to be a dedicated worker
 *                       thread rather than the OS main thread. This
 *                       mode exists so embedders (game loops, editors)
 *                       can reserve the OS main thread for platform
 *                       events and drive the engine from a worker.
 *   - @c Test        -- short-tick pump suitable for ctest smoke
 *                       scenarios. Behaves exactly like @c Foreground
 *                       under the engine's point of view; the enum
 *                       exists as a hint so future diagnostics leaves
 *                       can dial back logging when tests are running.
 */
enum class RunMode : std::uint8_t
{
    Foreground = 1,
    Background = 2,
    Test       = 3,
};

/**
 * @brief POD describing the shape of a freshly-built @ref IEngine.
 *
 * Passed to @ref createEngine at construction time. Carries the two
 * inputs the engine needs before it can wire the context aggregator
 * together:
 *
 *   1. @ref context configures the @ref IContext the engine will own.
 *      This is the single source of truth for the thread-manager and
 *      system-bus shape -- the engine does not duplicate those fields.
 *   2. @ref runMode hints at how @ref IEngine::run will hold the caller
 *      (see @ref RunMode).
 *
 * Services and user buses are NOT part of this struct. Callers register
 * them on the returned engine's @ref IEngine::context between
 * @ref createEngine and @ref IEngine::run; @ref IEngine::run calls
 * @ref IContext::freeze on entry, after which topology mutation is
 * rejected with @ref Result::Code::TopologyFrozen.
 */
struct EngineConfig
{
    /**
     * @brief Context aggregator configuration consumed first.
     */
    context::ContextConfig context{};

    /**
     * @brief Run-mode hint consumed by @ref IEngine::run.
     *
     * Defaults to @c Foreground so a bare default-constructed config
     * describes the canonical production engine.
     */
    RunMode runMode{RunMode::Foreground};
};

} // namespace vigine::engine

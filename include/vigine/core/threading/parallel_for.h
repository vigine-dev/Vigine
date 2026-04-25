#pragma once

#include <cstddef>
#include <functional>
#include <memory>

#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/core/threading/itaskhandle.h"
#include "vigine/core/threading/threadaffinity.h"

namespace vigine::core::threading
{
/**
 * @file
 * @brief Free helper that fans out a `[0, count)` integer range across a
 *        thread pool and returns an aggregated handle whose `wait()`
 *        blocks until every chunk has finished.
 *
 * The helper is a thin wrapper over @ref IThreadManager::schedule and is
 * intentionally not a method on @ref IThreadManager itself: keeping the
 * interface minimal lets implementations stay focused on dispatch
 * primitives, while higher-level patterns like fan-out + barrier live as
 * free functions on top.
 *
 * Chunk-size strategy: the range is split into `max(1, count / poolSize)`-
 * sized chunks so that each worker in the pool pulls roughly one chunk.
 * When `poolSize()` reports `0` (e.g. a degenerate manager with no
 * workers), the helper falls back to a single chunk covering the entire
 * range — the pool will still drain it, just serially.
 *
 * @param tm        Thread manager to dispatch chunks on.
 * @param count     Number of work items in `[0, count)`.
 * @param body      Per-index callback. Stored in a shared `std::function`
 *                  that outlives every dispatched chunk. Callers are
 *                  expected to keep captured references valid for the
 *                  lifetime of the returned handle.
 * @param affinity  @ref ThreadAffinity for chunk dispatch (default
 *                  @c Pool).
 *
 * @return Aggregated @ref ITaskHandle. @ref ITaskHandle::wait returns a
 *         successful @ref Result when every chunk completed without
 *         error; otherwise the first failing chunk's error is
 *         propagated.
 *
 * @note R-NoTemplates compliant: signature uses @c std::function, not a
 *       template parameter.
 */
[[nodiscard]] std::unique_ptr<ITaskHandle>
    parallelFor(IThreadManager                       &tm,
                std::size_t                           count,
                std::function<void(std::size_t index)> body,
                ThreadAffinity                        affinity = ThreadAffinity::Pool);

} // namespace vigine::core::threading

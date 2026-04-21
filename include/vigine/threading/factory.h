#pragma once

#include <memory>

#include "vigine/threading/ithreadmanager.h"
#include "vigine/threading/threadmanagerconfig.h"

namespace vigine::threading
{
/**
 * @brief Constructs the default @ref IThreadManager implementation.
 *
 * Returns a unique owning pointer to the concrete engine-provided
 * thread manager. The factory is deliberately non-templated; every
 * implementation is selected at build time by the engine library.
 *
 * The returned manager is already running: its worker pool is alive,
 * the main-thread queue is open, and named-thread registration is
 * accepted. Release the pointer (or call @ref IThreadManager::shutdown
 * explicitly) to stop the manager; the destructor enforces a clean
 * shutdown either way.
 *
 * A @c unique_ptr is used — not a @c shared_ptr — because the manager
 * is a singular owner inside the engine construction chain. Callers
 * that need shared ownership can downcast the returned pointer into a
 * @c shared_ptr at the call site.
 */
[[nodiscard]] std::unique_ptr<IThreadManager>
    createThreadManager(const ThreadManagerConfig &config = {});

} // namespace vigine::threading

#pragma once

#include <memory>

#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/core/threading/threadmanagerconfig.h"

namespace vigine::core::threading
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
 * that need shared ownership can transfer the returned pointer into a
 * @c shared_ptr at the call site
 * (`std::shared_ptr<IThreadManager>(std::move(uniquePtr))`). That is an
 * ownership transfer, not a downcast.
 */
[[nodiscard]] std::unique_ptr<IThreadManager>
    createThreadManager(const ThreadManagerConfig &config = {});

} // namespace vigine::core::threading

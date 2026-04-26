#include "vigine/core/threading/factory.h"

#include <memory>

#include "vigine/core/threading/threadmanager.h"
#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/core/threading/threadmanagerconfig.h"

namespace vigine::core::threading
{
// The factory is intentionally non-templated. unique_ptr ownership —
// not shared_ptr — because the thread manager is a singular owner
// inside the engine construction chain; callers that need shared
// ownership can lift the returned pointer into a shared_ptr at the
// call site.

std::unique_ptr<IThreadManager> createThreadManager(const ThreadManagerConfig &config)
{
    return std::make_unique<ThreadManager>(config);
}

} // namespace vigine::core::threading

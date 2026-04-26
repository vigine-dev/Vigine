#include "vigine/api/context/factory.h"

#include <memory>

#include "vigine/impl/context/context.h"

namespace vigine::context
{

// The factory is intentionally non-templated. unique_ptr ownership
// (FF-1) -- not shared_ptr -- because the context is a singular owner
// inside the engine construction chain; callers that need shared
// ownership can lift the returned pointer into a shared_ptr at the
// call site.

std::unique_ptr<IContext> createContext(const ContextConfig &config)
{
    return std::make_unique<Context>(config);
}

} // namespace vigine::context

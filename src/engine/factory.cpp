#include "vigine/api/engine/factory.h"

#include <memory>

#include "vigine/impl/engine/engine.h"

namespace vigine::engine
{

// The factory is intentionally non-templated. unique_ptr ownership
// (FF-1) -- not shared_ptr -- because the engine is a singular owner
// inside the engine construction chain; callers that need shared
// ownership can lift the returned pointer into a shared_ptr at the
// call site.

std::unique_ptr<IEngine> createEngine(const EngineConfig &config)
{
    return std::make_unique<Engine>(config);
}

} // namespace vigine::engine

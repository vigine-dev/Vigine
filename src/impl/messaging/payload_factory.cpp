#include "vigine/api/messaging/payload/factory.h"

#include <memory>

#include "impl/messaging/payloadregistry.h"

namespace vigine::payload
{
// Factory returns a unique_ptr because the payload registry is a
// singular owner inside the engine construction chain. Callers that
// need shared ownership can lift the returned pointer into a shared_ptr
// at the call site.

std::unique_ptr<IPayloadRegistry> createPayloadRegistry()
{
    return std::make_unique<PayloadRegistry>();
}

} // namespace vigine::payload

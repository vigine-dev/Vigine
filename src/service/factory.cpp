#include "vigine/service/factory.h"

#include <memory>

#include "service/defaultservice.h"

namespace vigine::service
{

std::unique_ptr<IService> createService()
{
    // The factory constructs the default concrete closer over
    // AbstractService. The returned object has the invalid-sentinel
    // ServiceId until it is registered with a container; callers that
    // need a real id go through the engine container in downstream
    // leaves.
    return std::make_unique<DefaultService>();
}

} // namespace vigine::service

#include "vigine/api/service/abstractservice.h"

#include <utility>

#include "impl/service/serviceregistry.h"

namespace vigine::service
{

AbstractService::AbstractService()
    : _registry{std::make_unique<ServiceRegistry>()}
{
}

AbstractService::~AbstractService() = default;

ServiceId AbstractService::id() const noexcept
{
    return _id;
}

Result AbstractService::onInit(IContext & /*context*/)
{
    // Default lifecycle: flip the flag. Concrete services chain up to
    // this implementation after they finish their own domain-specific
    // initialisation so the observed @ref isInitialised state matches
    // reality.
    setInitialised(true);
    return Result{};
}

Result AbstractService::onShutdown(IContext & /*context*/)
{
    // Default teardown: flip the flag and drop the cross-service
    // references so the dependency list does not outlive its purpose.
    // Dependency lifetime itself sits on the container (unique_ptr),
    // not on this vector — clearing it is purely a bookkeeping step.
    // Concrete services chain up after releasing their own domain
    // resources.
    setInitialised(false);
    _dependencies.clear();
    return Result{};
}

std::vector<IService *> AbstractService::dependencies() const
{
    return _dependencies;
}

bool AbstractService::isInitialised() const noexcept
{
    return _initialised.load(std::memory_order_acquire);
}

void AbstractService::setInitialised(bool value) noexcept
{
    _initialised.store(value, std::memory_order_release);
}

void AbstractService::addDependency(IService *dependency)
{
    if (!dependency)
        return;

    _dependencies.push_back(dependency);
}

void AbstractService::setId(ServiceId identifier) noexcept
{
    _id = identifier;
}

} // namespace vigine::service

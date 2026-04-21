#include "vigine/service/abstractservice.h"

#include <utility>

#include "service/serviceregistry.h"

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
    // Default teardown: flip the flag and clear the cross-service
    // ownership edges so a dependency target is not kept alive longer
    // than its dependent needs it. Concrete services chain up after
    // releasing their own domain resources.
    setInitialised(false);
    _dependencies.clear();
    return Result{};
}

std::vector<std::shared_ptr<IService>> AbstractService::dependencies() const
{
    return _dependencies;
}

bool AbstractService::isInitialised() const noexcept
{
    return _initialised;
}

void AbstractService::setInitialised(bool value) noexcept
{
    _initialised = value;
}

void AbstractService::addDependency(std::shared_ptr<IService> dependency)
{
    if (!dependency)
        return;

    _dependencies.push_back(std::move(dependency));
}

void AbstractService::setId(ServiceId identifier) noexcept
{
    _id = identifier;
}

} // namespace vigine::service

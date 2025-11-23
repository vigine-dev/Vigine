#include "vigine/service/platformservice.h"

#include "vigine/ecs/platform/windowsystem.h"

using namespace vigine::platform;

PlatformService::PlatformService(const Name &name) : AbstractService(name) {}

PlatformService::~PlatformService() = default;

void PlatformService::contextChanged() {}

void PlatformService::entityBound() {}

vigine::ServiceId PlatformService::id() const { return "Platform"; }

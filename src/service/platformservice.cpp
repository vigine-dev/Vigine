#include "vigine/service/platformservice.h"

using namespace vigine::platform;

PlatformService::PlatformService(const Name &name) : AbstractService(name) {}

void PlatformService::contextChanged() {}

void PlatformService::entityBound() {}

vigine::ServiceId PlatformService::id() const { return "Platform"; }

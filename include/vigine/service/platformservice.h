#pragma once

#include "vigine/abstractservice.h"
#include "vigine/base/macros.h"

namespace vigine
{
namespace platform
{
class PlatformService : public AbstractService
{
  public:
    PlatformService(const Name &name);

    ServiceId id() const override;

  protected:
    void contextChanged() override;
    void entityBound() override;
};

BUILD_SMART_PTR(PlatformService);

} // namespace platform
} // namespace vigine

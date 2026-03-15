#pragma once

#ifdef _WIN32

#include "windowcomponent.h"

namespace vigine
{
namespace platform
{
class WinAPIComponent : public WindowComponent
{
  public:
    void show() override;
};
} // namespace platform
} // namespace vigine

#endif // _WIN32

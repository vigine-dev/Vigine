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
    [[nodiscard]] bool isMouseTracking() const;
    void setMouseTracking(bool value);

  protected:
    void show() override;

  private:
    static WinAPIComponent *_instance;
    bool _isMouseTracking{false};
};
} // namespace platform
} // namespace vigine

#endif // _WIN32

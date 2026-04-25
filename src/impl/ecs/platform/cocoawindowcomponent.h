#pragma once

#ifdef __APPLE__

#include "windowcomponent.h"

#include <chrono>
#include <memory>

namespace vigine {
namespace platform {

class CocoaWindowComponent : public WindowComponent {
public:
  CocoaWindowComponent();
  ~CocoaWindowComponent() override;

  [[nodiscard]] void *nativeHandle() const override;

  // Called internally from the Cocoa delegate when the window closes.
  void handleWindowClose();
  // Called internally from the Cocoa delegate on resize.
  void handleWindowResize(int width, int height);

protected:
  void show() override;

private:
  bool ensureWindowCreated();
  void runFrame();

  // Pimpl: keeps all Objective-C types out of the header.
  struct Impl;
  std::unique_ptr<Impl> _impl;

  std::chrono::steady_clock::time_point _fpsSampleStart{};
  uint32_t _fpsFrameCount{0};
};

} // namespace platform
} // namespace vigine

#endif // __APPLE__

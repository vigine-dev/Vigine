#pragma once

#ifdef _WIN32

#include "windowcomponent.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <windows.h>

namespace vigine
{
namespace platform
{
class WinAPIComponent : public WindowComponent
{
  public:
    [[nodiscard]] bool isMouseTracking() const;
    void setMouseTracking(bool value);
    [[nodiscard]] void *nativeHandle() const override;
    void runFrame();
    void updateFpsOverlayPosition();
    void toggleOverlayVisibility();
    void setRenderedVertexCount(uint64_t vertexCount);

  protected:
    void show() override;

  private:
    bool ensureWindowCreated();
    void createFpsOverlay();
    void updateFpsOverlay();

    static WinAPIComponent *_instance;
    bool _isMouseTracking{false};
    HWND _windowHandle{nullptr};
    HWND _fpsLabelHandle{nullptr};
    std::chrono::steady_clock::time_point _fpsSampleStart{};
    uint32_t _fpsFrameCount{0};
    std::array<char, 64> _fpsText{"FPS: --"};
    std::array<char, 256> _gpuName{"Unknown"};
    std::array<char, 512> _overlayText{"FPS: --"};
    uint64_t _renderedVertexCount{0};
    bool _overlayVisible{true};
};
} // namespace platform
} // namespace vigine

#endif // _WIN32

#pragma once

/**
 * @file iwindowsystem.h
 * @brief Pure-virtual platform window-system abstraction (R-PlatformPortability skeleton).
 *
 * Engine-side window-system interface. Distinct from
 * @c api/ecs/platform/iwindoweventhandler.h, which is the ECS-
 * subscriber side of the same data flow:
 *
 *   - @ref IWindowSystem (this file) abstracts the per-platform
 *     window-system primitive: creating native windows, polling /
 *     pumping the event queue, querying display metrics. CPU-side,
 *     pre-ECS.
 *   - @c IWindowEventHandlerComponent (ECS side) is the consumer
 *     callback registered against a specific window component once a
 *     window has been created and bound to an entity.
 *
 * TODO(#293): Reconcile with @c api/ecs/platform/iwindoweventhandler.h
 * once the platform-side concretes (Win32 / Linux XCB / macOS Cocoa)
 * land in @c src/impl/platform/<X>/. A follow-up leaf will decide
 * whether one delegates to the other or they merge.
 *
 * INV-10 compliance: the @c I prefix marks a pure-virtual interface
 * with no state and no non-virtual method bodies.
 */

#include <cstdint>
#include <string_view>

namespace vigine
{
namespace platform
{

/**
 * @brief Opaque handle for a native OS window.
 *
 * The concrete value is platform-defined (HWND on Win32, xcb_window_t
 * on Linux, NSWindow* on macOS). Public callers treat it as an opaque
 * token and pass it back to the same @ref IWindowSystem instance.
 */
using NativeWindowHandle = std::uintptr_t;

/**
 * @brief Sentinel returned when a window cannot be created.
 */
inline constexpr NativeWindowHandle kInvalidWindowHandle = 0;

/**
 * @brief Initial-creation parameters for a platform window.
 *
 * Plain-data descriptor. The @c title pointer must remain valid for
 * the duration of the @ref IWindowSystem::createWindow call; the
 * implementation is responsible for any persistent copy.
 */
struct WindowDesc
{
    std::string_view title;
    int width{0};
    int height{0};
    int x{0};
    int y{0};
    bool resizable{true};
    bool visible{true};
};

/**
 * @brief Pure-virtual root interface for the platform window system.
 *
 * The contract is intentionally narrow: create / destroy windows,
 * pump the OS event queue, and report whether the system is alive
 * (i.e. has not received a quit signal). Per-platform concretes
 * implement these against the OS-level window-system primitive.
 */
class IWindowSystem
{
  public:
    virtual ~IWindowSystem() = default;

    /**
     * @brief Create a native window matching the given descriptor.
     *
     * @return Opaque handle on success; @ref kInvalidWindowHandle on
     *         failure.
     */
    [[nodiscard]] virtual NativeWindowHandle createWindow(const WindowDesc &desc) = 0;

    /**
     * @brief Destroy a previously-created window.
     *
     * @return true on success; false if the handle was already
     *         destroyed or never valid.
     */
    virtual bool destroyWindow(NativeWindowHandle handle) = 0;

    /**
     * @brief Pump pending OS events from the event queue.
     *
     * Returns immediately whether or not events were available. The
     * implementation dispatches each event to the appropriate
     * subscriber path (e.g.
     * @c IWindowEventHandlerComponent on the ECS side).
     */
    virtual void pollEvents() = 0;

    /**
     * @brief Whether the window system is still alive.
     *
     * Returns false once the OS has signalled application quit (e.g.
     * the user closed the last window, the system requested a
     * shutdown).
     */
    [[nodiscard]] virtual bool isAlive() const = 0;

    IWindowSystem(const IWindowSystem &)            = delete;
    IWindowSystem &operator=(const IWindowSystem &) = delete;
    IWindowSystem(IWindowSystem &&)                 = delete;
    IWindowSystem &operator=(IWindowSystem &&)      = delete;

  protected:
    IWindowSystem() = default;
};

} // namespace platform
} // namespace vigine

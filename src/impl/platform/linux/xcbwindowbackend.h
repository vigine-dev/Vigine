#pragma once

#if defined(__linux__)

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include <xcb/xcb.h>

namespace vigine::platform::linux_
{

/**
 * @brief XCB-based window backend for Linux (primary windowing, v1).
 *
 * Creates and manages a single XCB window of the requested size, pumps
 * the XCB event queue on a dedicated thread, and exposes the raw
 * xcb_connection_t* / xcb_window_t pair required for
 * VK_KHR_xcb_surface creation.
 *
 * Wayland is deferred to v1.1 (UD-7). SDL3 / GLFW are rejected per UD-7.
 *
 * INV-10: class name does not use I prefix — this is not a pure interface.
 * INV-1:  no template parameters.
 */
class XcbWindowBackend final
{
  public:
    /**
     * @brief Opens a connection to the X server and creates a window.
     *
     * @param title  Window title string (UTF-8).
     * @param width  Initial client-area width in pixels.
     * @param height Initial client-area height in pixels.
     *
     * On failure the object is left in a valid-but-unusable state;
     * isValid() returns false.
     */
    explicit XcbWindowBackend(const std::string &title,
                              uint32_t           width  = 800,
                              uint32_t           height = 600);

    ~XcbWindowBackend();

    XcbWindowBackend(const XcbWindowBackend &)            = delete;
    XcbWindowBackend &operator=(const XcbWindowBackend &) = delete;
    XcbWindowBackend(XcbWindowBackend &&)                 = delete;
    XcbWindowBackend &operator=(XcbWindowBackend &&)      = delete;

    /** @brief Returns true when the XCB connection and window are ready. */
    [[nodiscard]] bool isValid() const noexcept;

    /**
     * @brief Requests the window to close (sets the stop flag).
     *
     * Non-blocking. The event loop thread exits on its next iteration.
     */
    void close() noexcept;

    /**
     * @brief Returns the raw XCB connection pointer (never nullptr if isValid()). */
    [[nodiscard]] xcb_connection_t *connection() const noexcept;

    /**
     * @brief Returns the XCB window ID (valid when isValid()). */
    [[nodiscard]] xcb_window_t windowId() const noexcept;

    /** @brief Current client-area width in pixels. */
    [[nodiscard]] uint32_t width() const noexcept;

    /** @brief Current client-area height in pixels. */
    [[nodiscard]] uint32_t height() const noexcept;

  private:
    void eventLoop() noexcept;

    xcb_connection_t *_connection{nullptr};
    xcb_window_t      _window{0};
    xcb_atom_t        _wmDeleteAtom{0};
    uint32_t          _width{0};
    uint32_t          _height{0};

    std::atomic<bool> _stop{false};
    std::atomic<bool> _valid{false};
    std::thread       _eventThread;
};

} // namespace vigine::platform::linux_

#endif // __linux__

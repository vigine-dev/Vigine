#if defined(__linux__)

#include "xcbwindowbackend.h"

#include <cstring>
#include <iostream>

#include <xcb/xcb.h>

namespace vigine::platform::linux_
{

XcbWindowBackend::XcbWindowBackend(const std::string &title,
                                   uint32_t           width,
                                   uint32_t           height)
    : _width(width), _height(height)
{
    int screenNumber = 0;
    _connection = ::xcb_connect(nullptr, &screenNumber);
    if (!_connection || ::xcb_connection_has_error(_connection))
    {
        std::cerr << "XcbWindowBackend: failed to connect to X server" << std::endl;
        if (_connection)
        {
            ::xcb_disconnect(_connection);
            _connection = nullptr;
        }
        return;
    }

    const xcb_setup_t    *setup  = ::xcb_get_setup(_connection);
    xcb_screen_iterator_t iter   = ::xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screenNumber; ++i)
    {
        ::xcb_screen_next(&iter);
    }
    xcb_screen_t *screen = iter.data;
    if (!screen)
    {
        std::cerr << "XcbWindowBackend: no screen found" << std::endl;
        ::xcb_disconnect(_connection);
        _connection = nullptr;
        return;
    }

    _window = ::xcb_generate_id(_connection);

    const uint32_t eventMask = XCB_EVENT_MASK_KEY_PRESS
                             | XCB_EVENT_MASK_KEY_RELEASE
                             | XCB_EVENT_MASK_BUTTON_PRESS
                             | XCB_EVENT_MASK_BUTTON_RELEASE
                             | XCB_EVENT_MASK_POINTER_MOTION
                             | XCB_EVENT_MASK_STRUCTURE_NOTIFY
                             | XCB_EVENT_MASK_EXPOSURE;

    const uint32_t values[2] = {screen->black_pixel, eventMask};

    ::xcb_create_window(
        _connection,
        XCB_COPY_FROM_PARENT,          // depth
        _window,
        screen->root,                  // parent
        0, 0,                          // x, y
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        0,                             // border-width
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        screen->root_visual,
        XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
        values);

    // Set window title via WM_NAME property.
    ::xcb_change_property(
        _connection,
        XCB_PROP_MODE_REPLACE,
        _window,
        XCB_ATOM_WM_NAME,
        XCB_ATOM_STRING,
        8,
        static_cast<uint32_t>(title.size()),
        title.c_str());

    // Register WM_DELETE_WINDOW so the event loop can intercept close.
    xcb_intern_atom_cookie_t protoCookie =
        ::xcb_intern_atom(_connection, 1, 12, "WM_PROTOCOLS");
    xcb_intern_atom_cookie_t deleteCookie =
        ::xcb_intern_atom(_connection, 0, 16, "WM_DELETE_WINDOW");

    xcb_intern_atom_reply_t *protoReply =
        ::xcb_intern_atom_reply(_connection, protoCookie, nullptr);
    xcb_intern_atom_reply_t *deleteReply =
        ::xcb_intern_atom_reply(_connection, deleteCookie, nullptr);

    if (protoReply && deleteReply)
    {
        _wmDeleteAtom = deleteReply->atom;
        ::xcb_change_property(
            _connection,
            XCB_PROP_MODE_REPLACE,
            _window,
            protoReply->atom,
            XCB_ATOM_ATOM,
            32,
            1,
            &deleteReply->atom);
    }
    free(protoReply);   // NOLINT(cppcoreguidelines-no-malloc)
    free(deleteReply);  // NOLINT(cppcoreguidelines-no-malloc)

    ::xcb_map_window(_connection, _window);
    ::xcb_flush(_connection);

    _valid.store(true, std::memory_order_release);

    // Start the event-pump thread.
    _eventThread = std::thread([this] { eventLoop(); });
}

XcbWindowBackend::~XcbWindowBackend()
{
    close();

    if (_eventThread.joinable())
    {
        _eventThread.join();
    }

    if (_connection)
    {
        if (_window)
        {
            ::xcb_destroy_window(_connection, _window);
        }
        ::xcb_disconnect(_connection);
        _connection = nullptr;
    }
}

bool XcbWindowBackend::isValid() const noexcept
{
    return _valid.load(std::memory_order_acquire);
}

void XcbWindowBackend::close() noexcept
{
    _stop.store(true, std::memory_order_release);
}

xcb_connection_t *XcbWindowBackend::connection() const noexcept
{
    return _connection;
}

xcb_window_t XcbWindowBackend::windowId() const noexcept
{
    return _window;
}

uint32_t XcbWindowBackend::width() const noexcept
{
    return _width;
}

uint32_t XcbWindowBackend::height() const noexcept
{
    return _height;
}

void XcbWindowBackend::eventLoop() noexcept
{
    while (!_stop.load(std::memory_order_acquire))
    {
        xcb_generic_event_t *event = ::xcb_poll_for_event(_connection);
        if (!event)
        {
            // No event ready; yield to avoid spinning.
            std::this_thread::yield();
            continue;
        }

        const uint8_t responseType = event->response_type & ~0x80U;

        switch (responseType)
        {
            case XCB_CONFIGURE_NOTIFY:
            {
                const auto *cfg =
                    reinterpret_cast<const xcb_configure_notify_event_t *>(event);
                if (cfg->width > 0)  { _width  = cfg->width; }
                if (cfg->height > 0) { _height = cfg->height; }
                break;
            }
            case XCB_CLIENT_MESSAGE:
            {
                const auto *cm =
                    reinterpret_cast<const xcb_client_message_event_t *>(event);
                if (cm->data.data32[0] == _wmDeleteAtom)
                {
                    close();
                }
                break;
            }
            case XCB_DESTROY_NOTIFY:
                close();
                break;
            default:
                break;
        }

        free(event); // NOLINT(cppcoreguidelines-no-malloc)
    }
}

} // namespace vigine::platform::linux_

#endif // __linux__

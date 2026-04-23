#pragma once

#include <memory>

#include <vigine/ecs/platform/iwindoweventhandler.h>
#include <vigine/payload/payloadtypeid.h>
#include <vigine/signalemitter/isignalpayload.h>

/**
 * @file windoweventpayload.h
 * @brief Application-side payloads that carry Win32 window input events
 *        from the window task to an @ref ISignalPayload subscriber.
 *
 * The identifiers below sit in the user half of the @c PayloadTypeId
 * space (@c [0x10000 .. 0xFFFFFFFF]). They are picked from the first
 * block inside the window-example sub-range so that future example
 * payloads can extend contiguously without colliding with other user
 * code that also registers into the user half.
 */

/**
 * @brief Payload identifier for @ref MouseButtonDownPayload.
 *
 * User-range value; registered by the application's payload registry
 * before the bus accepts publishes or subscribes that carry it.
 */
inline constexpr vigine::payload::PayloadTypeId kMouseButtonDownPayloadTypeId{0x20101u};

/**
 * @brief Payload identifier for @ref KeyDownPayload.
 *
 * User-range value; registered by the application's payload registry
 * before the bus accepts publishes or subscribes that carry it.
 */
inline constexpr vigine::payload::PayloadTypeId kKeyDownPayloadTypeId{0x20102u};

/**
 * @brief Immutable payload describing a mouse-button-down event.
 *
 * Carries the button identifier and the client-area coordinates at the
 * moment the button went down. Fields are @c const and set at
 * construction time; the bus may deliver the same pointer to multiple
 * subscribers, so the observable state never changes after publish.
 */
class MouseButtonDownPayload final : public vigine::signalemitter::ISignalPayload
{
  public:
    MouseButtonDownPayload(vigine::platform::MouseButton button, int x, int y) noexcept
        : _button(button), _x(x), _y(y)
    {
    }

    ~MouseButtonDownPayload() override = default;

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        return kMouseButtonDownPayloadTypeId;
    }

    [[nodiscard]] std::unique_ptr<vigine::signalemitter::ISignalPayload>
        clone() const override
    {
        return std::make_unique<MouseButtonDownPayload>(_button, _x, _y);
    }

    [[nodiscard]] vigine::platform::MouseButton button() const noexcept { return _button; }
    [[nodiscard]] int                           x() const noexcept { return _x; }
    [[nodiscard]] int                           y() const noexcept { return _y; }

  private:
    const vigine::platform::MouseButton _button;
    const int                           _x;
    const int                           _y;
};

/**
 * @brief Immutable payload describing a key-down event.
 *
 * Wraps a copy of the originating @ref vigine::platform::KeyEvent so
 * that subscribers can read @c keyCode, @c scanCode, @c modifiers,
 * @c repeatCount, and @c isRepeat without reaching back into the
 * platform layer. The wrapped struct is held by value; exposing it by
 * @c const reference keeps the contract read-only.
 */
class KeyDownPayload final : public vigine::signalemitter::ISignalPayload
{
  public:
    explicit KeyDownPayload(const vigine::platform::KeyEvent &event) noexcept : _event(event) {}

    ~KeyDownPayload() override = default;

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        return kKeyDownPayloadTypeId;
    }

    [[nodiscard]] std::unique_ptr<vigine::signalemitter::ISignalPayload>
        clone() const override
    {
        return std::make_unique<KeyDownPayload>(_event);
    }

    [[nodiscard]] const vigine::platform::KeyEvent &event() const noexcept { return _event; }

  private:
    const vigine::platform::KeyEvent _event;
};

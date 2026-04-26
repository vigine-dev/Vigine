#pragma once

#include <memory>

#include <vigine/api/ecs/platform/iwindoweventhandler.h>
#include <vigine/api/messaging/payload/payloadtypeid.h>
#include <vigine/api/messaging/payload/isignalpayload.h>

/**
 * @file windoweventpayload.h
 * @brief Application-side payloads that carry Win32 window input events
 *        from the window task to an @ref ISignalPayload subscriber.
 *
 * The identifiers used to be hardcoded constants; they are now allocated
 * at runtime by the engine's payload-registry broker through
 * @c IPayloadRegistry::allocateId so the example never picks user-range
 * numbers itself. The two extern variables below get filled in by
 * @c main.cpp before @c engine->run() — every payload instance reads
 * its @c typeId() through the variable so emit / subscribe paths see the
 * registry-allocated id consistently.
 */

namespace example::payloads
{
/**
 * @brief Payload identifier for @ref MouseButtonDownPayload.
 *
 * Default-initialised to the invalid sentinel; @c main.cpp overwrites
 * it with the result of
 * @c context.payloadRegistry().allocateId("example-window.mouse.button.down")
 * before the engine pump starts.
 */
extern vigine::payload::PayloadTypeId mouseButtonDown;

/**
 * @brief Payload identifier for @ref KeyDownPayload.
 *
 * Same lifecycle as @ref mouseButtonDown — broker-allocated by
 * @c main.cpp under owner @c "example-window.key.down".
 */
extern vigine::payload::PayloadTypeId keyDown;
} // namespace example::payloads

/**
 * @brief Immutable payload describing a mouse-button-down event.
 *
 * Carries the button identifier and the client-area coordinates at the
 * moment the button went down. Fields are @c const and set at
 * construction time; the bus may deliver the same pointer to multiple
 * subscribers, so the observable state never changes after publish.
 */
class MouseButtonDownPayload final : public vigine::messaging::ISignalPayload
{
  public:
    MouseButtonDownPayload(vigine::ecs::platform::MouseButton button, int x, int y) noexcept
        : _button(button), _x(x), _y(y)
    {
    }

    ~MouseButtonDownPayload() override = default;

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        return example::payloads::mouseButtonDown;
    }

    [[nodiscard]] std::unique_ptr<vigine::messaging::ISignalPayload>
        clone() const override
    {
        return std::make_unique<MouseButtonDownPayload>(_button, _x, _y);
    }

    [[nodiscard]] vigine::ecs::platform::MouseButton button() const noexcept { return _button; }
    [[nodiscard]] int                           x() const noexcept { return _x; }
    [[nodiscard]] int                           y() const noexcept { return _y; }

  private:
    const vigine::ecs::platform::MouseButton _button;
    const int                           _x;
    const int                           _y;
};

/**
 * @brief Immutable payload describing a key-down event.
 *
 * Wraps a copy of the originating @ref vigine::ecs::platform::KeyEvent so
 * that subscribers can read @c keyCode, @c scanCode, @c modifiers,
 * @c repeatCount, and @c isRepeat without reaching back into the
 * platform layer. The wrapped struct is held by value; exposing it by
 * @c const reference keeps the contract read-only.
 */
class KeyDownPayload final : public vigine::messaging::ISignalPayload
{
  public:
    explicit KeyDownPayload(const vigine::ecs::platform::KeyEvent &event) noexcept : _event(event) {}

    ~KeyDownPayload() override = default;

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        return example::payloads::keyDown;
    }

    [[nodiscard]] std::unique_ptr<vigine::messaging::ISignalPayload>
        clone() const override
    {
        return std::make_unique<KeyDownPayload>(_event);
    }

    [[nodiscard]] const vigine::ecs::platform::KeyEvent &event() const noexcept { return _event; }

  private:
    const vigine::ecs::platform::KeyEvent _event;
};

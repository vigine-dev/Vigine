#pragma once

#include <memory>

#include <vigine/api/ecs/platform/iwindoweventhandler.h>
#include <vigine/api/messaging/payload/ipayloadregistry.h>
#include <vigine/api/messaging/payload/payloadtypeid.h>
#include <vigine/api/messaging/payload/isignalpayload.h>

/**
 * @file windoweventpayload.h
 * @brief Application-side payloads carrying Win32 window input events
 *        from the window task to an @ref ISignalPayload subscriber.
 *
 * Each payload class owns its @ref vigine::payload::PayloadTypeId
 * directly through a private @c static @c inline member. The id is
 * filled at startup by a one-shot @ref registerWith call that asks an
 * @ref vigine::payload::IPayloadRegistry to allocate a fresh user-range
 * id under a class-scoped owner string. Every instance returns the same
 * id from its @ref typeId override; subscribers compare against the
 * matching static getter (e.g. @ref staticTypeId).
 */

/**
 * @brief Immutable payload describing a mouse-button-down event.
 *
 * Fields are @c const and set at construction time; the bus may
 * deliver the same pointer to multiple subscribers, so the observable
 * state never changes after publish. The payload's @ref typeId is
 * class-scoped — see the class docstring on @ref windoweventpayload.h
 * for the registration story.
 */
class MouseButtonDownPayload final : public vigine::messaging::ISignalPayload
{
  public:
    MouseButtonDownPayload(vigine::ecs::platform::MouseButton button, int x, int y) noexcept
        : _button(button), _x(x), _y(y)
    {
    }

    ~MouseButtonDownPayload() override = default;

    /**
     * @brief Returns the registered class-scoped @c PayloadTypeId.
     *
     * Reads the @c static @c inline storage filled by @ref registerWith.
     * Returns the default-constructed (invalid) id when @ref registerWith
     * has not yet been called — callers depend on @ref registerWith
     * running before the first emit.
     */
    [[nodiscard]] static vigine::payload::PayloadTypeId staticTypeId() noexcept
    {
        return _staticTypeId;
    }

    /**
     * @brief Overwrites the class-scoped @c PayloadTypeId.
     *
     * Exposed alongside @ref registerWith for tests that want to
     * inject a deterministic id without going through the registry.
     * Production code calls @ref registerWith.
     */
    static void setStaticTypeId(vigine::payload::PayloadTypeId tid) noexcept
    {
        _staticTypeId = tid;
    }

    /**
     * @brief Allocates the class-scoped @c PayloadTypeId from
     *        @p registry under owner @c "example-window.mouse.button.down".
     *
     * One-shot — call once at startup before the first emit. A failed
     * allocation leaves the static id at the invalid sentinel; the
     * subsequent registry-validated emit will reject the payload as
     * unregistered, which is the desired loud failure.
     */
    static void registerWith(vigine::payload::IPayloadRegistry &registry)
    {
        if (auto id = registry.allocateId("example-window.mouse.button.down"))
            _staticTypeId = *id;
    }

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        return _staticTypeId;
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
    static inline vigine::payload::PayloadTypeId _staticTypeId{};

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
 * platform layer. Same class-scoped @c PayloadTypeId story as
 * @ref MouseButtonDownPayload.
 */
class KeyDownPayload final : public vigine::messaging::ISignalPayload
{
  public:
    explicit KeyDownPayload(const vigine::ecs::platform::KeyEvent &event) noexcept : _event(event) {}

    ~KeyDownPayload() override = default;

    [[nodiscard]] static vigine::payload::PayloadTypeId staticTypeId() noexcept
    {
        return _staticTypeId;
    }

    static void setStaticTypeId(vigine::payload::PayloadTypeId tid) noexcept
    {
        _staticTypeId = tid;
    }

    static void registerWith(vigine::payload::IPayloadRegistry &registry)
    {
        if (auto id = registry.allocateId("example-window.key.down"))
            _staticTypeId = *id;
    }

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        return _staticTypeId;
    }

    [[nodiscard]] std::unique_ptr<vigine::messaging::ISignalPayload>
        clone() const override
    {
        return std::make_unique<KeyDownPayload>(_event);
    }

    [[nodiscard]] const vigine::ecs::platform::KeyEvent &event() const noexcept { return _event; }

  private:
    static inline vigine::payload::PayloadTypeId _staticTypeId{};

    const vigine::ecs::platform::KeyEvent _event;
};

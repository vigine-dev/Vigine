#pragma once

#include <memory>
#include <string_view>

#include <vigine/api/ecs/platform/iwindoweventhandler.h>
#include <vigine/api/messaging/payload/payloadtypeid.h>
#include <vigine/api/messaging/payload/isignalpayload.h>

namespace vigine::payload
{
class IPayloadRegistry;
} // namespace vigine::payload

/**
 * @file windoweventpayload.h
 * @brief Application-side payloads carrying Win32 window input events
 *        from the window task to an @ref ISignalPayload subscriber.
 *
 * Each payload class declares @ref typeName — a @c static descriptor
 * that returns the registry type-name string used both as the
 * registration key and as the diagnostic label. The @c typeId()
 * virtual override on @ref ISignalPayload is implemented in the
 * matching .cpp by a lookup into a TU-local map
 * (@c payloadIdsByTypeName) populated once by
 * @ref example::payloads::registerAll. Callers that need class-level
 * access without an instance (signal-subscription registration in
 * @c main, dispatch comparison in @ref ProcessInputEventTask) go
 * through the namespace helper @ref example::payloads::idOf.
 */

/**
 * @brief Immutable payload describing a mouse-button-down event.
 */
class MouseButtonDownPayload final : public vigine::messaging::ISignalPayload
{
  public:
    /**
     * @brief Class-level type-name string. Definition lives in the .cpp;
     *        the literal does not appear in this header.
     */
    [[nodiscard]] static std::string_view typeName() noexcept;

    MouseButtonDownPayload(vigine::ecs::platform::MouseButton button, int x, int y) noexcept
        : _button(button), _x(x), _y(y)
    {
    }

    ~MouseButtonDownPayload() override = default;

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override;

    [[nodiscard]] std::unique_ptr<vigine::messaging::ISignalPayload>
        clone() const override;

    [[nodiscard]] vigine::ecs::platform::MouseButton button() const noexcept { return _button; }
    [[nodiscard]] int                                x() const noexcept { return _x; }
    [[nodiscard]] int                                y() const noexcept { return _y; }

  private:
    const vigine::ecs::platform::MouseButton _button;
    const int                                _x;
    const int                                _y;
};

/**
 * @brief Immutable payload describing a key-down event.
 */
class KeyDownPayload final : public vigine::messaging::ISignalPayload
{
  public:
    [[nodiscard]] static std::string_view typeName() noexcept;

    explicit KeyDownPayload(const vigine::ecs::platform::KeyEvent &event) noexcept : _event(event) {}

    ~KeyDownPayload() override = default;

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override;

    [[nodiscard]] std::unique_ptr<vigine::messaging::ISignalPayload>
        clone() const override;

    [[nodiscard]] const vigine::ecs::platform::KeyEvent &event() const noexcept { return _event; }

  private:
    const vigine::ecs::platform::KeyEvent _event;
};

namespace example::payloads
{
/**
 * @brief Allocates a fresh @c PayloadTypeId for every payload class
 *        defined in this file and stores the result in the TU-local
 *        lookup map.
 *
 * Call once from @c main before any @c signal() subscription that
 * resolves an id through @ref idOf — the lookup returns the invalid
 * sentinel for type-names not yet registered, so a missing
 * @ref registerAll call surfaces immediately as an unmatched
 * subscription.
 */
void registerAll(vigine::payload::IPayloadRegistry &registry);

/**
 * @brief Class-level access without an instance: returns the
 *        @c PayloadTypeId allocated for @p typeName, or the invalid
 *        sentinel when @p typeName has not been registered.
 *
 * Used by @c main.cpp / @c createInitTaskFlow for @c signal()
 * registration and by @c ProcessInputEventTask::onMessage for
 * dispatch comparison.
 */
[[nodiscard]] vigine::payload::PayloadTypeId
    idOf(std::string_view typeName) noexcept;
} // namespace example::payloads

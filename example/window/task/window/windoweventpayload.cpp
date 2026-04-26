#include "windoweventpayload.h"

#include <vigine/api/messaging/payload/ipayloadregistry.h>

#include <string_view>
#include <unordered_map>

namespace
{
/**
 * @brief TU-local map: each registered payload type-name string
 *        mapped to its allocated @c PayloadTypeId.
 *
 * Filled exactly once by @ref example::payloads::registerAll during
 * application startup (called from @c main before any @c signal()
 * subscription that depends on the ids); read-only during the engine
 * pump, so no synchronisation is needed.
 *
 * Key type is @c std::string_view so the lookup on the hot path
 * (every @c typeId() call from @c SignalMessage construction) does
 * not allocate. Safe because every inserted view comes from a
 * payload class's @c typeName(), which returns a @c string_view over
 * a string literal — the literal has static storage duration, so the
 * view outlives the map.
 */
std::unordered_map<std::string_view, vigine::payload::PayloadTypeId> payloadIdsByTypeName;

/**
 * @brief Returns the id stored for @p typeName, or the invalid
 *        sentinel when @p typeName has not been registered.
 *
 * Allocation-free on the hot path: the map is keyed by
 * @c std::string_view, so @c find(@p typeName) compares views
 * directly without materialising a temporary @c std::string.
 */
[[nodiscard]] vigine::payload::PayloadTypeId
    lookup(std::string_view typeName) noexcept
{
    auto it = payloadIdsByTypeName.find(typeName);
    return it != payloadIdsByTypeName.end()
               ? it->second
               : vigine::payload::PayloadTypeId{};
}
} // namespace

// ---------------------------------------------------------------------------
//  Type-name string definitions — kept out of the header so the literal lives
//  in exactly one translation unit.
// ---------------------------------------------------------------------------

std::string_view MouseButtonDownPayload::typeName() noexcept
{
    return "example-window.mouse.button.down";
}

std::string_view KeyDownPayload::typeName() noexcept
{
    return "example-window.key.down";
}

// ---------------------------------------------------------------------------
//  Virtual typeId() overrides — read from the lookup map keyed by typeName().
//  The bus reaches these through the @ref ISignalPayload* base pointer when
//  a SignalMessage is constructed; the cached value on the message then
//  drives every subscriber match.
// ---------------------------------------------------------------------------

vigine::payload::PayloadTypeId MouseButtonDownPayload::typeId() const noexcept
{
    return lookup(typeName());
}

vigine::payload::PayloadTypeId KeyDownPayload::typeId() const noexcept
{
    return lookup(typeName());
}

// ---------------------------------------------------------------------------
//  clone() — straightforward make_unique copies; defined here to keep the
//  header free of <memory> std::make_unique churn beyond the include.
// ---------------------------------------------------------------------------

std::unique_ptr<vigine::messaging::ISignalPayload>
MouseButtonDownPayload::clone() const
{
    return std::make_unique<MouseButtonDownPayload>(_button, _x, _y);
}

std::unique_ptr<vigine::messaging::ISignalPayload>
KeyDownPayload::clone() const
{
    return std::make_unique<KeyDownPayload>(_event);
}

// ---------------------------------------------------------------------------
//  Namespace API: registerAll fills the lookup map; idOf reads it back.
// ---------------------------------------------------------------------------

namespace example::payloads
{

vigine::payload::PayloadTypeId idOf(std::string_view typeName) noexcept
{
    return lookup(typeName);
}

void registerAll(vigine::payload::IPayloadRegistry &registry)
{
    auto allocateInto = [&](std::string_view name) {
        if (auto id = registry.allocateId(name))
            payloadIdsByTypeName[name] = *id;
    };
    allocateInto(MouseButtonDownPayload::typeName());
    allocateInto(KeyDownPayload::typeName());
}

} // namespace example::payloads

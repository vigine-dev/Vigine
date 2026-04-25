#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "vigine/api/messaging/payload/payloadtypeid.h"
#include "vigine/result.h"

namespace vigine::payload
{
/**
 * @brief Pure-virtual hybrid registry for @ref PayloadTypeId ranges.
 *
 * @ref IPayloadRegistry governs the 32-bit payload-id space. The engine
 * pre-registers four ranges at construction (Control, System, SystemExt,
 * Reserved — see @ref payloadrange.h); application code registers
 * additional ranges at runtime before first use. Every registration is
 * validated against the current table: overlap with an already-registered
 * range is rejected with @ref Result::Code::DuplicatePayloadId; an
 * ill-formed range (`min > max`) or one that straddles the engine-bundled
 * half (`[0, 0xFFFF]`) and the user half (`[0x10000, ...]`) is rejected
 * with @ref Result::Code::OutOfRange.
 *
 * Thread-safety: implementations must be safe to call from any thread.
 * Mutating entry points take an exclusive lock on an internal
 * reader-writer mutex; read-only entry points take a shared lock so
 * lookups on the hot path do not block each other. A visitor that
 * re-enters the registry from inside @ref resolve or @ref isRegistered
 * does not deadlock because those paths hold only a shared lock.
 *
 * Ownership: identifiers and owner strings are copied in by value at
 * registration time; callers never hand lifetime to the registry.
 */
class IPayloadRegistry
{
  public:
    virtual ~IPayloadRegistry() = default;

    /**
     * @brief Registers `[min, max]` (inclusive on both ends) under
     *        @p owner.
     *
     * Returns a success @ref Result when the range was installed cleanly.
     * Returns @ref Result::Code::DuplicatePayloadId when any identifier
     * in the requested range is already covered by another registration.
     * Returns @ref Result::Code::OutOfRange when @p min > @p max or when
     * the range straddles the engine-bundled half (`[0, 0xFFFF]`) and
     * the user-registered half (`[0x10000, ...]`). The two halves meet
     * at the 16-bit boundary — there is no unallocated "gap" between
     * them.
     *
     * @p owner is copied; an empty @p owner is a programming error and
     * the call is rejected with @ref Result::Code::OutOfRange.
     */
    [[nodiscard]] virtual Result
        registerRange(PayloadTypeId    min,
                      PayloadTypeId    max,
                      std::string_view owner) = 0;

    /**
     * @brief Returns the owner string associated with the range that
     *        contains @p id.
     *
     * Returns @c std::nullopt when @p id is not covered by any
     * registered range. Thread-safe: safe to call concurrently with
     * @ref registerRange and with other @ref resolve / @ref isRegistered
     * calls.
     */
    [[nodiscard]] virtual std::optional<std::string>
        resolve(PayloadTypeId id) const = 0;

    /**
     * @brief Returns @c true when @p id is covered by any registered
     *        range.
     *
     * Shorthand for @ref resolve with an @c .has_value() check; exists
     * so that hot paths avoid allocating the result string.
     */
    [[nodiscard]] virtual bool
        isRegistered(PayloadTypeId id) const noexcept = 0;

    /**
     * @brief Removes every range owned by @p owner, freeing those
     *        identifiers for future registration.
     *
     * Returns a success @ref Result even when @p owner holds no ranges;
     * callers drive this on dynamic-unload paths and do not need to
     * first check whether anything is registered.
     */
    [[nodiscard]] virtual Result
        unregister(std::string_view owner) = 0;

    IPayloadRegistry(const IPayloadRegistry &)            = delete;
    IPayloadRegistry &operator=(const IPayloadRegistry &) = delete;
    IPayloadRegistry(IPayloadRegistry &&)                 = delete;
    IPayloadRegistry &operator=(IPayloadRegistry &&)      = delete;

  protected:
    IPayloadRegistry() = default;
};

} // namespace vigine::payload

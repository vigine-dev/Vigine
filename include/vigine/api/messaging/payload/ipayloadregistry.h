#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "vigine/api/messaging/payload/payloadrange.h"
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

    /**
     * @brief Atomically allocates the first free contiguous block of
     *        @p count user-range identifiers and registers it under
     *        @p owner.
     *
     * The registry walks the user range (`[0x10000 .. 0xFFFFFFFF]`)
     * looking for the lowest gap of size at least @p count, registers
     * the resulting `[min, min + count - 1]` slice as if
     * @ref registerRange had been called, and returns a
     * @ref PayloadRange describing it. Returns @c std::nullopt when
     * no gap of the requested size exists or when @p count is zero.
     *
     * Callers receive an owned, validated id range; @ref resolve then
     * returns @p owner for any id inside it. The owner string itself
     * doubles as the semantic label — there is no separate
     * label-to-id reverse-lookup API.
     *
     * Thread-safety: the find-and-register step takes the registry's
     * exclusive lock so concurrent callers always see disjoint ranges.
     */
    [[nodiscard]] virtual std::optional<PayloadRange>
        allocateRange(std::uint32_t   count,
                      std::string_view owner) = 0;

    /**
     * @brief Convenience shorthand for @ref allocateRange with
     *        `count == 1`.
     *
     * Returns the single allocated id or @c std::nullopt when the
     * user range is full. Equivalent to
     * `allocateRange(1, owner).transform([](auto r) { return r.min; })`.
     */
    [[nodiscard]] virtual std::optional<PayloadTypeId>
        allocateId(std::string_view owner) = 0;

    /**
     * @brief Diagnostic dump: lists every `(owner, range)` pair whose
     *        owner string starts with @p ownerPrefix.
     *
     * An empty prefix lists every registered range — including the
     * engine-bundled ones under owner @c "vigine.core". A non-empty
     * prefix scopes the dump to one application or namespace
     * (typically the same string passed to @ref allocateId /
     * @ref allocateRange / @ref registerRange).
     *
     * The returned vector is unsorted; callers that need a specific
     * order sort it themselves. Thread-safe under the registry's
     * shared lock so concurrent diagnostic dumps do not block each
     * other.
     */
    [[nodiscard]] virtual std::vector<std::pair<std::string, PayloadRange>>
        labelsOf(std::string_view ownerPrefix = std::string_view{}) const = 0;

    IPayloadRegistry(const IPayloadRegistry &)            = delete;
    IPayloadRegistry &operator=(const IPayloadRegistry &) = delete;
    IPayloadRegistry(IPayloadRegistry &&)                 = delete;
    IPayloadRegistry &operator=(IPayloadRegistry &&)      = delete;

  protected:
    IPayloadRegistry() = default;
};

} // namespace vigine::payload

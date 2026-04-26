#include "api/messaging/payload/abstractpayloadregistry.h"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "vigine/api/messaging/payload/payloadrange.h"
#include "vigine/api/messaging/payload/payloadtypeid.h"
#include "vigine/result.h"

namespace vigine::payload
{
namespace
{
// Returns true when the range `[aMin, aMax]` overlaps with `[bMin, bMax]`
// assuming both are non-empty and inclusive on both ends.
[[nodiscard]] bool rangesOverlap(std::uint32_t aMin,
                                 std::uint32_t aMax,
                                 std::uint32_t bMin,
                                 std::uint32_t bMax) noexcept
{
    return aMin <= bMax && bMin <= aMax;
}

// Returns true when the range `[min, max]` sits entirely inside the
// engine-bundled half (`[0, kReservedEnd]`). Only `max` participates
// in the check — the range is well-formed (min <= max) by the
// caller's invariant, so an upper-bound check suffices. The
// `min` parameter is kept for signature symmetry with
// `isEntirelyUserHalf` so the two call sites read uniformly.
[[nodiscard]] bool isEntirelyEngineHalf([[maybe_unused]] std::uint32_t min,
                                        std::uint32_t                 max) noexcept
{
    return max <= kReservedEnd;
}

// Returns true when the range `[min, max]` sits entirely inside the
// user-registered half (`[kUserBegin, ..)`). Only `min` participates —
// the upper bound is unconstrained in the user half. The `max`
// parameter is kept for signature symmetry with
// `isEntirelyEngineHalf`.
[[nodiscard]] bool isEntirelyUserHalf(std::uint32_t                 min,
                                      [[maybe_unused]] std::uint32_t max) noexcept
{
    return min >= kUserBegin;
}

} // namespace

AbstractPayloadRegistry::AbstractPayloadRegistry() = default;

AbstractPayloadRegistry::~AbstractPayloadRegistry() = default;

void AbstractPayloadRegistry::bootstrapEngineRanges()
{
    // Runs on the constructing thread before the instance is handed
    // out, so no locking is needed. The helper still goes through
    // registerRangeLocked so every invariant check lives in one place.
    //
    // Results MUST be checked. The engine-range constants are adjacent,
    // non-overlapping, and sit inside the engine half by construction,
    // so these calls cannot fail under a valid configuration — but
    // that is exactly why a silent failure would be disastrous. If a
    // future change drifts the constants (e.g. `kSystemExtEnd >=
    // kReservedBegin`) the registry would ship with engine ranges
    // missing and every later `registerRange()` that lands inside
    // those ranges would succeed where it should have failed. Escalate
    // through `std::logic_error` so the registry construction halts
    // deterministically with a locatable stack trace, rather than
    // leaving the engine in a broken-invariant state.
    const auto check = [](const Result &r, const char *label) {
        if (!r.isSuccess())
        {
            throw std::logic_error(
                std::string{"payload: engine bootstrap failed for "} +
                label + " range: " + std::string{r.message()});
        }
    };
    check(registerRangeLocked(kControlBegin,   kControlEnd,   kEngineOwner), "Control");
    check(registerRangeLocked(kSystemBegin,    kSystemEnd,    kEngineOwner), "System");
    check(registerRangeLocked(kSystemExtBegin, kSystemExtEnd, kEngineOwner), "SystemExt");
    check(registerRangeLocked(kReservedBegin,  kReservedEnd,  kEngineOwner), "Reserved");
}

Result AbstractPayloadRegistry::registerRange(PayloadTypeId    min,
                                              PayloadTypeId    max,
                                              std::string_view owner)
{
    std::unique_lock<std::shared_mutex> lock(_mutex);
    return registerRangeLocked(min.value, max.value, owner);
}

Result AbstractPayloadRegistry::registerRangeLocked(std::uint32_t    min,
                                                    std::uint32_t    max,
                                                    std::string_view owner)
{
    if (owner.empty())
    {
        return Result{Result::Code::OutOfRange, "empty owner string"};
    }
    if (min > max)
    {
        return Result{Result::Code::OutOfRange, "inverted range (min > max)"};
    }
    // Reject ranges that span the reserved/user gap or that bridge the
    // engine-bundled half and the user-registered half. A valid range is
    // either entirely in the engine half or entirely in the user half.
    const bool engineSide = isEntirelyEngineHalf(min, max);
    const bool userSide   = isEntirelyUserHalf(min, max);
    if (!engineSide && !userSide)
    {
        return Result{Result::Code::OutOfRange,
                      "range crosses the engine/user boundary"};
    }

    if (overlapsAnyLocked(min, max))
    {
        return Result{Result::Code::DuplicatePayloadId,
                      "range overlaps an already-registered range"};
    }

    RangeEntry entry;
    entry.min   = min;
    entry.max   = max;
    entry.owner = std::string{owner};
    _ranges.push_back(std::move(entry));
    return Result{};
}

std::optional<std::string>
    AbstractPayloadRegistry::resolve(PayloadTypeId id) const
{
    std::shared_lock<std::shared_mutex> lock(_mutex);
    const RangeEntry *entry = findContainingLocked(id.value);
    if (entry == nullptr)
    {
        return std::nullopt;
    }
    return entry->owner;
}

bool AbstractPayloadRegistry::isRegistered(PayloadTypeId id) const noexcept
{
    // `noexcept` by contract — if the shared-lock construction or any
    // downstream call somehow throws (std::bad_alloc from an exotic
    // allocator, a custom-mutex deadlock trap, etc.) the runtime
    // translates the escape into `std::terminate` per the standard.
    // That is loud and locatable. The previous `catch(...)` branch
    // turned every unexpected exception into a silent `false`, which
    // collapsed real bugs into wrong answers the caller could not
    // distinguish from a genuine "unregistered" result.
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return findContainingLocked(id.value) != nullptr;
}

Result AbstractPayloadRegistry::unregister(std::string_view owner)
{
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _ranges.erase(
        std::remove_if(_ranges.begin(), _ranges.end(),
                       [&](const RangeEntry &entry) { return entry.owner == owner; }),
        _ranges.end());
    return Result{};
}

bool AbstractPayloadRegistry::overlapsAnyLocked(std::uint32_t min,
                                                std::uint32_t max) const noexcept
{
    for (const RangeEntry &entry : _ranges)
    {
        if (rangesOverlap(entry.min, entry.max, min, max))
        {
            return true;
        }
    }
    return false;
}

const AbstractPayloadRegistry::RangeEntry *
    AbstractPayloadRegistry::findContainingLocked(std::uint32_t value) const noexcept
{
    for (const RangeEntry &entry : _ranges)
    {
        if (entry.min <= value && value <= entry.max)
        {
            return &entry;
        }
    }
    return nullptr;
}

std::optional<std::uint32_t>
    AbstractPayloadRegistry::findFirstFreeBlockLocked(std::uint32_t count) const noexcept
{
    if (count == 0u)
        return std::nullopt;

    // Snapshot the user-half entries sorted by min; engine-half entries
    // cannot overlap with a user-range allocation so they are ignored
    // for the gap scan.
    std::vector<std::pair<std::uint32_t, std::uint32_t>> sorted;
    sorted.reserve(_ranges.size());
    for (const RangeEntry &entry : _ranges)
    {
        if (entry.min >= kUserBegin)
            sorted.emplace_back(entry.min, entry.max);
    }
    std::sort(sorted.begin(), sorted.end());

    std::uint32_t cursor = kUserBegin;
    for (const auto &[mn, mx] : sorted)
    {
        if (mn > cursor)
        {
            const std::uint32_t gap = mn - cursor;
            if (gap >= count)
                return cursor;
        }
        // Advance past this range. When mx already covers the very
        // top of the user half, no further gap exists; bail early to
        // avoid the mx+1 overflow in the cursor update below.
        if (mx == 0xFFFFFFFFu)
            return std::nullopt;
        cursor = (std::max)(cursor, mx + 1u);
    }

    // Final gap [cursor .. 0xFFFFFFFF] inclusive — width is computed
    // in 64-bit so the +1 cannot overflow when cursor == 0.
    const std::uint64_t finalWidth =
        static_cast<std::uint64_t>(0xFFFFFFFFu) - cursor + 1u;
    if (finalWidth >= count)
        return cursor;
    return std::nullopt;
}

std::optional<PayloadRange>
    AbstractPayloadRegistry::allocateRange(std::uint32_t   count,
                                           std::string_view owner)
{
    // Up-front argument validation. Both checks mirror the contract
    // documented on @ref IPayloadRegistry::allocateRange so callers can
    // branch on a null optional without having to query the underlying
    // @ref Result.
    if (count == 0u)
        return std::nullopt;
    if (owner.empty())
        return std::nullopt;

    std::unique_lock<std::shared_mutex> lock(_mutex);
    const auto baseOpt = findFirstFreeBlockLocked(count);
    if (!baseOpt)
        return std::nullopt;

    const std::uint32_t minId = *baseOpt;
    const std::uint32_t maxId = minId + count - 1u;

    if (!registerRangeLocked(minId, maxId, owner).isSuccess())
    {
        // findFirstFreeBlockLocked guarantees no overlap, the count
        // check above guarantees min <= max, and owner is non-empty —
        // so the registration cannot fail under a valid invariant.
        // Return nullopt as a defensive fallback so callers see one
        // unified failure surface even if a future code path drifts
        // the invariant.
        return std::nullopt;
    }

    return PayloadRange{PayloadTypeId{minId}, PayloadTypeId{maxId}};
}

std::optional<PayloadTypeId>
    AbstractPayloadRegistry::allocateId(std::string_view owner)
{
    const auto rangeOpt = allocateRange(1u, owner);
    if (!rangeOpt)
        return std::nullopt;
    return rangeOpt->min;
}

std::vector<std::pair<std::string, PayloadRange>>
    AbstractPayloadRegistry::labelsOf(std::string_view ownerPrefix) const
{
    std::shared_lock<std::shared_mutex> lock(_mutex);
    std::vector<std::pair<std::string, PayloadRange>> out;
    out.reserve(_ranges.size());
    for (const RangeEntry &entry : _ranges)
    {
        if (ownerPrefix.empty() ||
            std::string_view{entry.owner}.starts_with(ownerPrefix))
        {
            out.emplace_back(
                entry.owner,
                PayloadRange{PayloadTypeId{entry.min}, PayloadTypeId{entry.max}});
        }
    }
    return out;
}

} // namespace vigine::payload

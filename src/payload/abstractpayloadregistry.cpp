#include "payload/abstractpayloadregistry.h"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "vigine/payload/payloadrange.h"
#include "vigine/payload/payloadtypeid.h"
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
// engine-bundled half (`[0, kReservedEnd]`).
[[nodiscard]] bool isEntirelyEngineHalf(std::uint32_t min,
                                        std::uint32_t max) noexcept
{
    return max <= kReservedEnd;
}

// Returns true when the range `[min, max]` sits entirely inside the
// user-registered half (`[kUserBegin, ..)`).
[[nodiscard]] bool isEntirelyUserHalf(std::uint32_t min,
                                      std::uint32_t max) noexcept
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

} // namespace vigine::payload

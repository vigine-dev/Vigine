#include "payload/abstractpayloadregistry.h"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
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
    static_cast<void>(registerRangeLocked(kControlBegin, kControlEnd, kEngineOwner));
    static_cast<void>(registerRangeLocked(kSystemBegin, kSystemEnd, kEngineOwner));
    static_cast<void>(registerRangeLocked(kSystemExtBegin, kSystemExtEnd, kEngineOwner));
    static_cast<void>(registerRangeLocked(kReservedBegin, kReservedEnd, kEngineOwner));
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
    try
    {
        std::shared_lock<std::shared_mutex> lock(_mutex);
        return findContainingLocked(id.value) != nullptr;
    }
    catch (...)
    {
        // Honouring the noexcept surface on the pure-virtual: a failed
        // shared-lock acquisition translates to a conservative "not
        // registered" answer rather than a crash.
        return false;
    }
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

#pragma once

#include <cstdint>

namespace vigine::channelfactory
{

/**
 * @brief Closed enum describing the capacity behaviour of a channel.
 *
 * @ref ChannelKind controls how @ref IChannelFactory::create interprets
 * the capacity argument:
 *
 *   - @c Bounded   -- the channel holds at most @c capacity messages; the
 *                     capacity argument must be >= 1 or
 *                     @ref vigine::Result::Code::Error is returned.
 *   - @c Unbounded -- the channel grows without limit; the capacity argument
 *                     must be 0 (callers pass 0 to signal "do not care") or
 *                     @ref vigine::Result::Code::Error is returned.
 *
 * Invariants:
 *   - INV-11: no graph types appear in this header.
 *   - The enum is closed (no sentinel / max value) to prevent accidental
 *     numeric casts leaking from call sites.
 */
enum class ChannelKind : std::uint8_t
{
    Bounded   = 0,
    Unbounded = 1,
};

} // namespace vigine::channelfactory

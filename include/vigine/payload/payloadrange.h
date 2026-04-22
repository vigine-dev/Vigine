#pragma once

#include <cstdint>

namespace vigine::payload
{
/**
 * @brief Reserved payload-id range constants.
 *
 * The 32-bit @c PayloadTypeId space is split into two halves:
 *
 *   - `[0 .. 0xFFFF]` — engine-bundled. Pre-registered at registry
 *     construction; owned by the engine. This half is further carved
 *     into four sub-ranges so that different engine concerns do not
 *     collide with each other:
 *       * Control    `[0x0000 .. 0x00FF]` — low-volume engine control
 *                       payloads (lifecycle, shutdown, tick).
 *       * System     `[0x0100 .. 0x0FFF]` — core engine subsystems
 *                       (graph, threading, ECS, state machine,
 *                       task flow).
 *       * SystemExt  `[0x1000 .. 0x7FFF]` — extension subsystems
 *                       shipped with the engine but not part of the
 *                       core surface.
 *       * Reserved   `[0x8000 .. 0xFFFF]` — reserved for future engine
 *                       use. Application code must not register into
 *                       this range.
 *
 *   - `[0x10000 .. 0xFFFFFFFF]` — user-registered. Available for
 *     application-defined payload types. Every registration in this
 *     half is performed at runtime through
 *     @ref IPayloadRegistry::registerRange.
 *
 * `kReservedEnd = 0xFFFF` and `kUserBegin = 0x10000` are adjacent:
 * the engine-bundled half covers `[0x0000 .. 0xFFFF]` and the user
 * half starts immediately after at `0x10000`. There is no gap between
 * them — the split is at the 16-bit boundary.
 */

inline constexpr std::uint32_t kControlBegin   = 0x0000u;
inline constexpr std::uint32_t kControlEnd     = 0x00FFu;

inline constexpr std::uint32_t kSystemBegin    = 0x0100u;
inline constexpr std::uint32_t kSystemEnd      = 0x0FFFu;

inline constexpr std::uint32_t kSystemExtBegin = 0x1000u;
inline constexpr std::uint32_t kSystemExtEnd   = 0x7FFFu;

inline constexpr std::uint32_t kReservedBegin  = 0x8000u;
inline constexpr std::uint32_t kReservedEnd    = 0xFFFFu;

inline constexpr std::uint32_t kUserBegin      = 0x10000u;

/**
 * @brief Stable owner label used for every engine-bundled range
 *        pre-registered at registry construction. Application code can
 *        inspect the owner string returned by
 *        @ref IPayloadRegistry::resolve to tell engine-owned identifiers
 *        apart from user-owned ones.
 */
inline constexpr const char *kEngineOwner = "vigine.core";

} // namespace vigine::payload

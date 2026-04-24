#pragma once

/**
 * @file function.h
 * @brief Small free helpers shared across the engine base layer.
 */

namespace vigine
{
/**
 * @brief Static cast wrapper with named intent.
 *
 * Convenience helper that forwards its argument through a @c static_cast to
 * the target type @c T. Using this helper instead of a raw cast preserves
 * the call-site intent ("convert to T") and makes the conversion easy to
 * grep for.
 *
 * @tparam T  Target type produced by the cast.
 * @tparam T2 Source type deduced from the argument.
 * @param  type Value to convert.
 * @return The argument converted to @c T via @c static_cast.
 */
template <typename T, typename T2>
T to(T2 type)
{
    return static_cast<T>(type);
}
} // namespace vigine

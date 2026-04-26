#pragma once

/**
 * @file constant.h
 * @brief Engine-wide numeric constants used by the base layer.
 */

namespace vigine
{
/**
 * @brief First numeric id reserved for user-defined types.
 *
 * Engine-owned type ids live in the closed range @c [0, UserType); ids at or
 * above this value are free for downstream code to assign. The value is a
 * compile-time constant so it can participate in template dispatch and
 * @c switch labels without ODR concerns.
 */
constexpr int UserType = 256;
}

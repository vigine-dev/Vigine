#pragma once

namespace vigine
{
template <typename T, typename T2>
T to(T2 type)
{
    return static_cast<T>(type);
}
} // namespace vigine

#pragma once

#include <cstddef>
#include <cstdint>

#include "vigine/core/graph/nodeid.h"

namespace vigine::core::graph::internal
{
// ---------------------------------------------------------------------------
// Hash adapter so NodeId can be a key in unordered containers.
//
// The hasher folds the (index, generation) pair into a single size_t:
//
//   1. Build the full 64-bit key from index (high 32 bits) | generation
//      (low 32 bits). Both halves are widened to uint64_t before the
//      shift, which dodges the [expr.shift] UB that hits a literal
//      "<< 32" on a 32-bit operand (the shift amount would equal the
//      promoted width).
//
//   2. On 64-bit platforms the key fits directly in size_t.
//      On 32-bit platforms we fold the high half into the low half
//      (XOR) before narrowing so both halves contribute to the bucket.
//
// Used in both traverse and query drivers — centralised here so the
// implementation does not drift between sites.
// ---------------------------------------------------------------------------
struct NodeIdHasher
{
    std::size_t operator()(NodeId id) const noexcept
    {
        const std::uint64_t key =
            (static_cast<std::uint64_t>(id.index) << 32)
          | static_cast<std::uint64_t>(id.generation);
        if constexpr (sizeof(std::size_t) >= sizeof(std::uint64_t))
        {
            return static_cast<std::size_t>(key);
        }
        else
        {
            return static_cast<std::size_t>(key ^ (key >> 32));
        }
    }
};
} // namespace vigine::core::graph::internal

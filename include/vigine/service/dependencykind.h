#pragma once

#include <cstdint>

namespace vigine::service
{
/**
 * @brief Classifies how a declared service dependency is resolved.
 *
 * Closed enum; extensions are a breaking API change under the architect
 * gate. The wrapper consumes this enum only when building the
 * topological-init DAG; it is not used on the dispatch hot path.
 */
enum class DependencyKind : std::uint8_t
{
    /// Must be resolved before @ref IService::onInit runs; the engine
    /// aborts the init chain if the handle cannot be satisfied.
    Required = 1,

    /// The dependency is resolved if present but its absence is not
    /// fatal; the service proceeds with @ref IService::onInit and
    /// null-checks the handle at use sites.
    Optional = 2,

    /// The dependency is not required at init; the service resolves the
    /// handle lazily on first use.
    RuntimeOnly = 3,
};

} // namespace vigine::service

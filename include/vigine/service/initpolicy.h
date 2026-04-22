#pragma once

#include <cstdint>

namespace vigine::service
{
/**
 * @brief Selects when the engine calls @ref IService::onInit on a
 *        registered service.
 *
 * Closed enum; extensions are a breaking API change under the architect
 * gate. The choice is made per-service by the registering code; the
 * engine honours it when walking the topological order.
 */
enum class InitPolicy : std::uint8_t
{
    /// @ref IService::onInit runs on the first call that resolves the
    /// service through the container. Useful for heavy services an
    /// application may not need in every run.
    Lazy = 1,

    /// @ref IService::onInit runs during engine startup, in the
    /// topological order computed from @ref IService::dependencies. The
    /// default for services whose startup costs are acceptable.
    Eager = 2,

    /// @ref IService::onInit is not called automatically; the owner
    /// triggers it explicitly through the engine. Reserved for services
    /// with bespoke initialisation rules (long-running handshakes,
    /// external broker wait loops).
    Manual = 3,
};

} // namespace vigine::service

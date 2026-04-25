#pragma once

#include <chrono>

namespace vigine::requestbus
{

/**
 * @brief Per-request configuration POD for @ref IRequestBus::request.
 *
 * @ref RequestConfig bundles the two time-bound tunables that govern one
 * request/response pair:
 *
 *   - @c timeout -- how long @ref IFuture::wait may block before
 *     returning @c std::nullopt. The default is the maximum
 *     representable milliseconds (effectively "wait forever").
 *   - @c ttl    -- how long the bus keeps the correlation id alive
 *     after the future's wait window. A zero @c ttl means "use the
 *     default", which is @c timeout * 2. A non-zero @c ttl is honoured
 *     verbatim (UD-5, Q-MF4 configurable TTL).
 *
 * TTL semantics:
 *   - After TTL expiration the bus invalidates the correlation id and
 *     drops any arriving late reply silently (logged at debug level).
 *   - Setting @c ttl == @c timeout gives no grace period after the
 *     caller has given up waiting -- useful for strict single-shot RPC.
 *   - Setting @c ttl > @c timeout allows late-reply diagnostics while
 *     still protecting memory from unbounded growth.
 *
 * Invariants:
 *   - POD aggregate: trivially constructible, copyable.
 *   - INV-11: no graph types appear in this header.
 */
struct RequestConfig
{
    /// How long @ref IFuture::wait blocks. Default: effectively forever.
    std::chrono::milliseconds timeout{std::chrono::milliseconds::max()};

    /// How long the bus keeps the correlation id alive after @c timeout.
    /// Zero is the sentinel meaning "default = timeout * 2".
    std::chrono::milliseconds ttl{std::chrono::milliseconds::zero()};
};

} // namespace vigine::requestbus

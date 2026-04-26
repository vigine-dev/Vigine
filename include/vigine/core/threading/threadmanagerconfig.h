#pragma once

#include <cstddef>

namespace vigine::core::threading
{
/**
 * @brief Configuration POD consumed by the threading factory.
 *
 * Captures the sizes of the two lazily-grown resource pools (worker pool
 * and the dedicated-thread registry) and the maximum number of named
 * threads the manager accepts. Every field has a default so that a
 * caller that does not care about tuning can pass a
 * default-constructed instance.
 *
 * Hardware-concurrency defaults: the factory translates a @c 0 value for
 * @ref poolSize into `std::thread::hardware_concurrency()`, falling back
 * to @c 1 when the standard library reports @c 0. The same translation
 * applies to @ref maxDedicatedThreads; @ref maxNamedThreads defaults to
 * an upper bound the typical engine will never exhaust.
 */
// ENCAP EXEMPT: pure value aggregate
struct ThreadManagerConfig
{
    /**
     * @brief Number of worker threads in the pool served by
     *        @ref ThreadAffinity::Pool and @ref ThreadAffinity::Any.
     *
     * A value of @c 0 asks the factory to derive the pool size from
     * `std::thread::hardware_concurrency()`; if the runtime reports @c 0
     * (rare), the factory uses @c 1.
     */
    std::size_t poolSize{0};

    /**
     * @brief Upper bound on concurrent dedicated threads the manager will
     *        create on demand.
     *
     * A value of @c 0 asks the factory to derive the limit from
     * `std::thread::hardware_concurrency()` so that the manager will
     * never hand out more dedicated threads than the hardware can service
     * in parallel. Any positive value overrides the derivation.
     */
    std::size_t maxDedicatedThreads{0};

    /**
     * @brief Upper bound on named threads the manager will register.
     *
     * Defaults to a value well above realistic usage. Setting a smaller
     * value is a form of resource capping for embedded or test harnesses.
     */
    std::size_t maxNamedThreads{1024};
};

} // namespace vigine::core::threading

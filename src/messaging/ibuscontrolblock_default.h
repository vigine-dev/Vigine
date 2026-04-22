#pragma once

#include <atomic>
#include <cstdint>
#include <shared_mutex>
#include <unordered_map>

#include "vigine/messaging/connectionid.h"
#include "vigine/messaging/ibuscontrolblock.h"

namespace vigine::messaging
{
class AbstractMessageTarget;

/**
 * @brief Reference implementation of @ref IBusControlBlock used by the
 *        default message bus.
 *
 * The class owns the atomic alive flag, the generational slot
 * generator, and the target registry. Mutating entry points take an
 * exclusive lock on an internal @c std::shared_mutex; the alive check
 * is wait-free through a @c std::atomic<bool> loaded with acquire
 * ordering. The registry keeps raw, non-owning pointers to
 * @ref AbstractMessageTarget objects; lifetime safety is provided by
 * @ref ConnectionToken running the corresponding @ref unregisterTarget
 * before the target is destroyed.
 *
 * The class is sized for a single bus instance. It lives under @c src
 * because it is a concrete implementation detail; the public surface
 * for callers is @ref IBusControlBlock.
 */
class DefaultBusControlBlock final : public IBusControlBlock
{
  public:
    DefaultBusControlBlock() noexcept;
    ~DefaultBusControlBlock() override;

    [[nodiscard]] bool isAlive() const noexcept override;
    void               markDead() noexcept override;

    [[nodiscard]] ConnectionId
        allocateSlot(AbstractMessageTarget *target) override;
    void unregisterTarget(ConnectionId id) noexcept override;

    /**
     * @brief RAII pair holding the registry's shared lock alongside
     *        a target pointer.
     *
     * Returned by @ref lookup so callers can dereference `target`
     * without racing a concurrent `unregisterTarget`. The shared
     * lock releases when the guard goes out of scope. Move-only
     * (the shared_lock is move-only by construction); copies would
     * accidentally extend the locked region indefinitely.
     *
     * Usage:
     *   if (auto guard = control.lookup(id); guard.target != nullptr)
     *   {
     *       guard.target->onMessage(msg);
     *   }
     *
     * When the lookup fails (invalid id, stale generation, bus
     * dead), `guard.target` is `nullptr` and the shared_lock is
     * not held. Callers only need to null-check `target`.
     */
    struct LookupGuard
    {
        AbstractMessageTarget               *target{nullptr};
        std::shared_lock<std::shared_mutex>  lock{};
    };

    /**
     * @brief Returns an RAII guard around the live target pointer
     *        behind @p id, or an empty guard when the slot is stale,
     *        recycled, or the bus is dead.
     *
     * The returned guard owns a shared_lock on the registry, so
     * dereferencing `guard.target` is safe against a concurrent
     * `unregisterTarget` for the lifetime of the guard. Releasing
     * the guard releases the lock.
     */
    [[nodiscard]] LookupGuard lookup(ConnectionId id) const noexcept;

  private:
    struct Slot
    {
        AbstractMessageTarget *target{nullptr};
        std::uint32_t          generation{0};
    };

    mutable std::shared_mutex                     _registryMutex;
    std::unordered_map<std::uint32_t, Slot>       _registry;
    std::uint32_t                                 _nextIndex{1};
    std::uint32_t                                 _nextGeneration{1};
    std::atomic<bool>                             _alive{true};
};

} // namespace vigine::messaging

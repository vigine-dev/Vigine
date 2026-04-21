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
     * @brief Returns the live target pointer behind @p id, or
     *        @c nullptr when the slot is stale, recycled, or the bus
     *        is dead.
     *
     * Exposed on the concrete type so that the bus's dispatch path can
     * look up the target without reaching into the registry map
     * directly. The returned pointer is valid only as long as the
     * matching @ref ConnectionToken is alive; the bus holds a
     * @c shared_lock while dereferencing it.
     */
    [[nodiscard]] AbstractMessageTarget *lookup(ConnectionId id) const noexcept;

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

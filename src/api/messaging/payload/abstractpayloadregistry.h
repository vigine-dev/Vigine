#pragma once

#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

#include "vigine/api/messaging/payload/ipayloadregistry.h"
#include "vigine/api/messaging/payload/payloadtypeid.h"
#include "vigine/result.h"

namespace vigine::payload
{
/**
 * @brief Concrete stateful base for every in-process @ref IPayloadRegistry.
 *
 * @ref AbstractPayloadRegistry carries the shared state every concrete
 * payload registry needs: the reader-writer mutex, the range table, and
 * the overlap-detection helpers. It implements the full
 * @ref IPayloadRegistry surface so that every concrete subclass
 * (currently @c PayloadRegistry) only has to choose its storage
 * representation — the lookup and validation logic is identical across
 * implementations and lives here.
 *
 * The class carries state, so it follows the project's @c Abstract
 * naming convention rather than the @c I pure-virtual prefix. It is
 * abstract only in the logical sense — users do not instantiate it
 * directly; @ref createPayloadRegistry returns a @c final subclass that
 * closes the inheritance chain.
 *
 * Thread-safety: every mutating entry point takes an exclusive lock on
 * an internal @c std::shared_mutex; every read-only entry point takes
 * a shared lock. Concurrent @c resolve / @c isRegistered calls do not
 * block each other.
 */
class AbstractPayloadRegistry : public IPayloadRegistry
{
  public:
    ~AbstractPayloadRegistry() override;

    [[nodiscard]] Result
        registerRange(PayloadTypeId    min,
                      PayloadTypeId    max,
                      std::string_view owner) override;

    [[nodiscard]] std::optional<std::string>
        resolve(PayloadTypeId id) const override;

    [[nodiscard]] bool
        isRegistered(PayloadTypeId id) const noexcept override;

    [[nodiscard]] Result
        unregister(std::string_view owner) override;

    [[nodiscard]] std::optional<PayloadRange>
        allocateRange(std::uint32_t   count,
                      std::string_view owner) override;

    [[nodiscard]] std::optional<PayloadTypeId>
        allocateId(std::string_view owner) override;

    [[nodiscard]] std::vector<std::pair<std::string, PayloadRange>>
        labelsOf(std::string_view ownerPrefix = std::string_view{}) const override;

  protected:
    AbstractPayloadRegistry();

    /**
     * @brief Pre-registers the four engine-bundled ranges under the
     *        `vigine.core` owner.
     *
     * Invoked by the constructor of the concrete subclass so that the
     * vtable is fully formed by the time the initial registrations run.
     * Because the call happens on the constructing thread before the
     * instance is handed out, no locking is required.
     */
    void bootstrapEngineRanges();

  private:
    struct RangeEntry
    {
        std::uint32_t min{0};
        std::uint32_t max{0};
        std::string   owner;
    };

    // Internal registration helper that assumes the exclusive lock is
    // already held. Used both by @ref registerRange and by
    // @ref bootstrapEngineRanges (the bootstrap path is single-threaded
    // but funnelling through one insertion routine keeps the invariants
    // in exactly one place).
    [[nodiscard]] Result registerRangeLocked(std::uint32_t    min,
                                             std::uint32_t    max,
                                             std::string_view owner);

    [[nodiscard]] bool overlapsAnyLocked(std::uint32_t min,
                                         std::uint32_t max) const noexcept;

    [[nodiscard]] const RangeEntry *
        findContainingLocked(std::uint32_t value) const noexcept;

    /**
     * @brief Walks the user range looking for the first gap of size at
     *        least @p count and returns its lower bound, or
     *        @c std::nullopt when no such gap exists.
     *
     * Caller must hold the exclusive lock; the helper relies on the
     * range table being stable for the duration of the scan and on the
     * follow-up @ref registerRangeLocked happening under the same
     * lock so concurrent @ref allocateRange calls do not race for the
     * same gap.
     */
    [[nodiscard]] std::optional<std::uint32_t>
        findFirstFreeBlockLocked(std::uint32_t count) const noexcept;

    mutable std::shared_mutex _mutex;
    std::vector<RangeEntry>   _ranges;
};

} // namespace vigine::payload

#include "vigine/messaging/factory.h"

#include <atomic>
#include <memory>

#include "vigine/messaging/busconfig.h"
#include "vigine/messaging/busid.h"
#include "vigine/messaging/systemmessagebus.h"

namespace vigine::messaging
{

namespace
{
// The system bus takes BusId == 1 (see SystemMessageBus::systemPreset);
// the factory hands out ids starting from 2 to user buses so every live
// bus carries a unique value. The counter is static to the factory
// translation unit and survives across calls; two buses built by the
// same process always end up with distinct ids, which matters for
// facades that route by bus id.
std::atomic<std::uint32_t> &busIdCounter() noexcept
{
    static std::atomic<std::uint32_t> counter{2};
    return counter;
}

[[nodiscard]] BusConfig resolveBusId(const BusConfig &input) noexcept
{
    BusConfig resolved = input;
    if (!resolved.id.valid())
    {
        resolved.id = BusId{busIdCounter().fetch_add(1, std::memory_order_relaxed)};
    }
    return resolved;
}
} // namespace

std::unique_ptr<IMessageBus>
    createMessageBus(const BusConfig                  &config,
                     vigine::core::threading::IThreadManager &threadManager)
{
    BusConfig resolved = resolveBusId(config);
    return std::make_unique<SystemMessageBus>(std::move(resolved), threadManager);
}

} // namespace vigine::messaging

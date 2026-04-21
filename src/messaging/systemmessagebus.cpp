#include "vigine/messaging/systemmessagebus.h"

#include <utility>

#include "vigine/messaging/busconfig.h"
#include "vigine/messaging/busid.h"

namespace vigine::messaging
{

namespace
{
// Preset config for the engine's system bus. The engine reserves
// BusId == 1 for the system tier so facades can find it without a
// string lookup. Priority::High + ThreadingPolicy::Dedicated keep
// lifecycle traffic responsive under application load (UD-5 resolves
// Q-MB2 toward "shared by default, dedicated for system").
[[nodiscard]] constexpr BusConfig systemPreset() noexcept
{
    return BusConfig{
        /* id           */ BusId{1},
        /* name         */ std::string_view{"system-bus"},
        /* priority     */ BusPriority::High,
        /* threading    */ ThreadingPolicy::Dedicated,
        /* capacity     */ QueueCapacity{1024, true},
        /* backpressure */ BackpressurePolicy::Block,
    };
}
} // namespace

SystemMessageBus::SystemMessageBus(vigine::threading::IThreadManager &threadManager)
    : AbstractMessageBus{systemPreset(), threadManager}
{
}

SystemMessageBus::SystemMessageBus(BusConfig                          config,
                                   vigine::threading::IThreadManager &threadManager)
    : AbstractMessageBus{std::move(config), threadManager}
{
}

} // namespace vigine::messaging

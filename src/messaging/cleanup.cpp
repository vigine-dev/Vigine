#include "messaging/cleanup.h"

namespace vigine::messaging
{

void MessagingCleanup::flipDead(
    const std::shared_ptr<IBusControlBlock> &block) noexcept
{
    if (!block)
    {
        return;
    }
    block->markDead();
}

} // namespace vigine::messaging

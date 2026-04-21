// Factory bodies for sync-primitive creators on AbstractThreadManager.
//
// The definitions live in a dedicated TU rather than inside
// abstractthreadmanager.cpp so that extending the primitive catalogue
// in a later leaf does not force a recompile of the registry/lifecycle
// bookkeeping code. DefaultThreadManager inherits these definitions
// unchanged — it does not override any of them.

#include <cstddef>
#include <memory>

#include "defaultbarrier.h"
#include "defaultmessagechannel.h"
#include "defaultmutex.h"
#include "defaultsemaphore.h"
#include "vigine/threading/abstractthreadmanager.h"
#include "vigine/threading/ibarrier.h"
#include "vigine/threading/imessagechannel.h"
#include "vigine/threading/imutex.h"
#include "vigine/threading/isemaphore.h"

namespace vigine::threading
{
std::unique_ptr<IMutex> AbstractThreadManager::createMutex()
{
    return std::make_unique<DefaultMutex>();
}

std::unique_ptr<ISemaphore>
AbstractThreadManager::createSemaphore(std::size_t initialCount)
{
    return std::make_unique<DefaultSemaphore>(initialCount);
}

std::unique_ptr<IBarrier>
AbstractThreadManager::createBarrier(std::size_t parties)
{
    return std::make_unique<DefaultBarrier>(parties);
}

std::unique_ptr<IMessageChannel>
AbstractThreadManager::createMessageChannel(std::size_t capacity)
{
    return std::make_unique<DefaultMessageChannel>(capacity);
}

} // namespace vigine::threading

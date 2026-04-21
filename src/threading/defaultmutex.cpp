#include "defaultmutex.h"

#include <chrono>

namespace vigine::threading
{
DefaultMutex::DefaultMutex() noexcept = default;

DefaultMutex::~DefaultMutex() = default;

Result DefaultMutex::lock(std::chrono::milliseconds timeout)
{
    // std::chrono::milliseconds::max() is the "block forever" sentinel;
    // forwarding that to try_lock_for would overflow the internal
    // timeout computation on some implementations. The infinite branch
    // delegates to std::timed_mutex::lock which blocks until acquired.
    if (timeout == std::chrono::milliseconds::max())
    {
        _mutex.lock();
        return Result{Result::Code::Success};
    }

    if (_mutex.try_lock_for(timeout))
    {
        return Result{Result::Code::Success};
    }
    return Result{Result::Code::Error, "threading: mutex lock timeout"};
}

bool DefaultMutex::tryLock()
{
    return _mutex.try_lock();
}

void DefaultMutex::unlock()
{
    _mutex.unlock();
}

} // namespace vigine::threading

#include "defaultsemaphore.h"

#include <chrono>
#include <mutex>

namespace vigine::threading
{
DefaultSemaphore::DefaultSemaphore(std::size_t initialCount) noexcept
    : _count{initialCount}
{
}

DefaultSemaphore::~DefaultSemaphore() = default;

Result DefaultSemaphore::acquire(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(_mutex);
    if (timeout == std::chrono::milliseconds::max())
    {
        _cv.wait(lock, [this] { return _count > 0; });
        --_count;
        return Result{Result::Code::Success};
    }

    if (!_cv.wait_for(lock, timeout, [this] { return _count > 0; }))
    {
        return Result{Result::Code::Error, "threading: semaphore acquire timeout"};
    }
    --_count;
    return Result{Result::Code::Success};
}

bool DefaultSemaphore::tryAcquire()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_count == 0)
    {
        return false;
    }
    --_count;
    return true;
}

void DefaultSemaphore::release()
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        ++_count;
    }
    _cv.notify_one();
}

std::size_t DefaultSemaphore::count() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _count;
}

} // namespace vigine::threading

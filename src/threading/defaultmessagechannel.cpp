#include "defaultmessagechannel.h"

#include <chrono>
#include <mutex>
#include <utility>

namespace vigine::threading
{
namespace
{
// Clamp capacity: 0 degenerates into a 1-slot channel so call sites do
// not deadlock or miscompute full/empty state.
[[nodiscard]] std::size_t clampCapacity(std::size_t capacity) noexcept
{
    return capacity == 0 ? std::size_t{1} : capacity;
}
} // namespace

DefaultMessageChannel::DefaultMessageChannel(std::size_t capacity) noexcept
    : _capacity{clampCapacity(capacity)}, _closed{false}
{
}

DefaultMessageChannel::~DefaultMessageChannel()
{
    // Deterministic shutdown: wake every waiter and drain. Destroying
    // a channel with pending senders/receivers is a caller error, but
    // calling close here makes the dtor safe rather than blocking on a
    // condition variable.
    close();
}

Result DefaultMessageChannel::send(Message message, std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(_mutex);
    const auto predicate = [this] { return _queue.size() < _capacity || _closed; };

    if (timeout == std::chrono::milliseconds::max())
    {
        _notFull.wait(lock, predicate);
    }
    else if (!_notFull.wait_for(lock, timeout, predicate))
    {
        return Result{Result::Code::Error, "threading: channel send timeout"};
    }

    if (_closed)
    {
        return Result{Result::Code::Error, "threading: channel closed"};
    }

    _queue.push_back(std::move(message));
    lock.unlock();
    _notEmpty.notify_one();
    return Result{Result::Code::Success};
}

bool DefaultMessageChannel::trySend(Message &message)
{
    std::unique_lock<std::mutex> lock(_mutex);
    if (_closed || _queue.size() >= _capacity)
    {
        return false;
    }
    _queue.push_back(std::move(message));
    lock.unlock();
    _notEmpty.notify_one();
    return true;
}

Result DefaultMessageChannel::receive(Message &out, std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(_mutex);
    const auto predicate = [this] { return !_queue.empty() || _closed; };

    if (timeout == std::chrono::milliseconds::max())
    {
        _notEmpty.wait(lock, predicate);
    }
    else if (!_notEmpty.wait_for(lock, timeout, predicate))
    {
        // Contract: on failure @p out is left default-constructed so
        // callers looping with a shared Message variable do not see
        // stale data from a previous receive.
        out = Message{};
        return Result{Result::Code::Error, "threading: channel receive timeout"};
    }

    if (_queue.empty())
    {
        // Closed and empty — final-drain path. Reset @p out per contract.
        out = Message{};
        return Result{Result::Code::Error, "threading: channel closed"};
    }

    out = std::move(_queue.front());
    _queue.pop_front();
    lock.unlock();
    _notFull.notify_one();
    return Result{Result::Code::Success};
}

bool DefaultMessageChannel::tryReceive(Message &out)
{
    std::unique_lock<std::mutex> lock(_mutex);
    if (_queue.empty())
    {
        return false;
    }
    out = std::move(_queue.front());
    _queue.pop_front();
    lock.unlock();
    _notFull.notify_one();
    return true;
}

void DefaultMessageChannel::close()
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_closed)
        {
            return;
        }
        _closed = true;
    }
    // Wake every waiter so they re-evaluate the closed flag and exit
    // with an error Result (for senders) or drain-then-error (for
    // receivers).
    _notFull.notify_all();
    _notEmpty.notify_all();
}

bool DefaultMessageChannel::isClosed() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _closed;
}

std::size_t DefaultMessageChannel::size() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _queue.size();
}

std::size_t DefaultMessageChannel::capacity() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _capacity;
}

} // namespace vigine::threading

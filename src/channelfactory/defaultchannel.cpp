#include "channelfactory/defaultchannel.h"

#include <chrono>
#include <utility>

namespace vigine::channelfactory
{

DefaultChannel::DefaultChannel(ChannelKind                    kind,
                                std::size_t                    capacity,
                                vigine::payload::PayloadTypeId expectedTypeId)
    : _kind(kind)
    , _capacity(capacity)
    , _expectedTypeId(expectedTypeId)
{
}

DefaultChannel::~DefaultChannel()
{
    close();
}

vigine::Result
DefaultChannel::send(std::unique_ptr<vigine::messaging::IMessagePayload> payload,
                     int timeoutMs)
{
    if (!payload)
    {
        return vigine::Result{vigine::Result::Code::Error, "null payload"};
    }

    if (payload->typeId() != _expectedTypeId)
    {
        return vigine::Result{vigine::Result::Code::Error, "payload type id mismatch"};
    }

    std::unique_lock<std::mutex> lock(_mutex);

    auto deadline = std::chrono::steady_clock::now() +
                    (timeoutMs < 0
                         ? std::chrono::milliseconds::max()
                         : std::chrono::milliseconds{timeoutMs});

    if (_kind == ChannelKind::Bounded)
    {
        // Block until a slot is available, the channel closes, or the
        // deadline elapses.
        bool slotAvailable = _notFull.wait_until(lock, deadline, [this] {
            return _closed.load(std::memory_order_relaxed) ||
                   _queue.size() < _capacity;
        });

        if (!slotAvailable)
        {
            return vigine::Result{vigine::Result::Code::Error, "send timed out"};
        }
    }

    if (_closed.load(std::memory_order_relaxed))
    {
        return vigine::Result{vigine::Result::Code::Error, "channel is closed"};
    }

    _queue.push(std::move(payload));
    lock.unlock();
    _notEmpty.notify_one();

    return vigine::Result{vigine::Result::Code::Success};
}

bool DefaultChannel::trySend(
    std::unique_ptr<vigine::messaging::IMessagePayload> &payload)
{
    if (!payload)
    {
        return false;
    }

    if (payload->typeId() != _expectedTypeId)
    {
        return false;
    }

    std::unique_lock<std::mutex> lock(_mutex);

    if (_closed.load(std::memory_order_relaxed))
    {
        return false;
    }

    if (_kind == ChannelKind::Bounded && _queue.size() >= _capacity)
    {
        return false;
    }

    _queue.push(std::move(payload));
    lock.unlock();
    _notEmpty.notify_one();
    return true;
}

vigine::Result
DefaultChannel::receive(std::unique_ptr<vigine::messaging::IMessagePayload> &out,
                        int timeoutMs)
{
    std::unique_lock<std::mutex> lock(_mutex);

    auto deadline = std::chrono::steady_clock::now() +
                    (timeoutMs < 0
                         ? std::chrono::milliseconds::max()
                         : std::chrono::milliseconds{timeoutMs});

    bool itemAvailable = _notEmpty.wait_until(lock, deadline, [this] {
        return !_queue.empty() || _closed.load(std::memory_order_relaxed);
    });

    if (!itemAvailable)
    {
        return vigine::Result{vigine::Result::Code::Error, "receive timed out"};
    }

    if (_queue.empty())
    {
        // Channel is closed and drained.
        return vigine::Result{vigine::Result::Code::Error, "channel is closed"};
    }

    out = std::move(_queue.front());
    _queue.pop();
    lock.unlock();
    _notFull.notify_one();

    return vigine::Result{vigine::Result::Code::Success};
}

bool DefaultChannel::tryReceive(
    std::unique_ptr<vigine::messaging::IMessagePayload> &out)
{
    std::unique_lock<std::mutex> lock(_mutex);

    if (_queue.empty())
    {
        return false;
    }

    out = std::move(_queue.front());
    _queue.pop();
    lock.unlock();
    _notFull.notify_one();
    return true;
}

void DefaultChannel::close()
{
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_closed.exchange(true, std::memory_order_acq_rel))
        {
            return; // already closed — idempotent
        }
    }
    // Wake all waiters so they can observe the closed state.
    _notFull.notify_all();
    _notEmpty.notify_all();
}

bool DefaultChannel::isClosed() const
{
    return _closed.load(std::memory_order_acquire);
}

std::size_t DefaultChannel::size() const
{
    std::unique_lock<std::mutex> lock(_mutex);
    return _queue.size();
}

} // namespace vigine::channelfactory

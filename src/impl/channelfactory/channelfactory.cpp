#include "vigine/impl/channelfactory/channelfactory.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "channelfactory/defaultchannel.h"
#include "vigine/api/channelfactory/channelkind.h"
#include "vigine/api/channelfactory/factory.h"
#include "vigine/api/channelfactory/ichannel.h"
#include "vigine/api/messaging/payload/payloadtypeid.h"
#include "vigine/result.h"

namespace vigine::channelfactory
{

// -----------------------------------------------------------------
// FactoryRegistry — shared state between the factory and its channels.
// Lives as a shared_ptr so that ChannelGuard can hold a weak_ptr and
// safely unregister even after the factory is destroyed.
// -----------------------------------------------------------------

struct FactoryRegistry
{
    mutable std::mutex      mutex;
    std::vector<IChannel *> liveChannels;   // non-owning raw pointers
    std::atomic<bool>       shutdown{false};

    void registerChannel(IChannel *ch)
    {
        std::unique_lock<std::mutex> lock(mutex);
        liveChannels.push_back(ch);
    }

    void unregisterChannel(IChannel *ch)
    {
        std::unique_lock<std::mutex> lock(mutex);
        auto it = std::find(liveChannels.begin(), liveChannels.end(), ch);
        if (it != liveChannels.end())
        {
            liveChannels.erase(it);
        }
    }
};

// -----------------------------------------------------------------
// ChannelGuard — IChannel wrapper that:
//   1. Delegates every IChannel call to the underlying DefaultChannel.
//   2. Unregisters from the FactoryRegistry on destruction (safe even
//      after the factory is gone, via weak_ptr).
// -----------------------------------------------------------------

class ChannelGuard final : public IChannel
{
  public:
    ChannelGuard(std::unique_ptr<DefaultChannel>    inner,
                 std::shared_ptr<FactoryRegistry>   registry)
        : _inner(std::move(inner))
        , _registry(std::move(registry))
    {
    }

    ~ChannelGuard() override
    {
        if (auto reg = _registry.lock())
        {
            reg->unregisterChannel(this);
        }
        // _inner is destroyed here; its dtor calls close() idempotently.
    }

    [[nodiscard]] vigine::Result
        send(std::unique_ptr<vigine::messaging::IMessagePayload> payload,
             int timeoutMs) override
    {
        return _inner->send(std::move(payload), timeoutMs);
    }

    [[nodiscard]] bool
        trySend(std::unique_ptr<vigine::messaging::IMessagePayload> &payload) override
    {
        return _inner->trySend(payload);
    }

    [[nodiscard]] vigine::Result
        receive(std::unique_ptr<vigine::messaging::IMessagePayload> &out,
                int timeoutMs) override
    {
        return _inner->receive(out, timeoutMs);
    }

    [[nodiscard]] bool
        tryReceive(std::unique_ptr<vigine::messaging::IMessagePayload> &out) override
    {
        return _inner->tryReceive(out);
    }

    void close() override { _inner->close(); }

    [[nodiscard]] bool         isClosed() const override { return _inner->isClosed(); }
    [[nodiscard]] std::size_t  size()     const override { return _inner->size(); }

    ChannelGuard(const ChannelGuard &)            = delete;
    ChannelGuard &operator=(const ChannelGuard &) = delete;
    ChannelGuard(ChannelGuard &&)                 = delete;
    ChannelGuard &operator=(ChannelGuard &&)      = delete;

  private:
    std::unique_ptr<DefaultChannel>    _inner;
    std::weak_ptr<FactoryRegistry>     _registry;
};

// -----------------------------------------------------------------
// ChannelFactory::Impl — holds the shared FactoryRegistry.
// -----------------------------------------------------------------

struct ChannelFactory::Impl
{
    std::shared_ptr<FactoryRegistry> registry;

    explicit Impl() : registry(std::make_shared<FactoryRegistry>()) {}
};

// -----------------------------------------------------------------
// ChannelFactory
// -----------------------------------------------------------------

ChannelFactory::ChannelFactory(vigine::messaging::IMessageBus &bus)
    : AbstractChannelFactory{bus}
    , _impl(std::make_unique<Impl>())
{
}

ChannelFactory::~ChannelFactory()
{
    shutdown();
}

std::unique_ptr<IChannel>
ChannelFactory::create(ChannelKind                    kind,
                               std::size_t                    capacity,
                               vigine::payload::PayloadTypeId expectedTypeId,
                               vigine::Result                *outResult)
{
    if (_impl->registry->shutdown.load(std::memory_order_acquire))
    {
        if (outResult)
        {
            *outResult = vigine::Result{vigine::Result::Code::Error,
                                        "channel factory is shut down"};
        }
        return nullptr;
    }

    // Config validation per plan_18 edge cases.
    if (kind == ChannelKind::Bounded && capacity < 1)
    {
        if (outResult)
        {
            *outResult = vigine::Result{vigine::Result::Code::Error,
                                        "Bounded channel requires capacity >= 1"};
        }
        return nullptr;
    }

    if (kind == ChannelKind::Unbounded && capacity != 0)
    {
        if (outResult)
        {
            *outResult = vigine::Result{vigine::Result::Code::Error,
                                        "Unbounded channel requires capacity == 0"};
        }
        return nullptr;
    }

    auto inner = std::make_unique<DefaultChannel>(kind, capacity, expectedTypeId);
    auto guard = std::make_unique<ChannelGuard>(std::move(inner), _impl->registry);

    _impl->registry->registerChannel(guard.get());

    if (outResult)
    {
        *outResult = vigine::Result{vigine::Result::Code::Success};
    }

    return guard;
}

vigine::Result ChannelFactory::shutdown()
{
    bool expected = false;
    if (!_impl->registry->shutdown.compare_exchange_strong(
            expected, true,
            std::memory_order_acq_rel,
            std::memory_order_acquire))
    {
        return vigine::Result{vigine::Result::Code::Success};
    }

    // Close every channel still alive so blocking threads wake up.
    std::unique_lock<std::mutex> lock(_impl->registry->mutex);
    for (IChannel *ch : _impl->registry->liveChannels)
    {
        ch->close();
    }

    return vigine::Result{vigine::Result::Code::Success};
}

// -----------------------------------------------------------------
// Factory function
// -----------------------------------------------------------------

std::unique_ptr<IChannelFactory>
createChannelFactory(vigine::messaging::IMessageBus &bus)
{
    return std::make_unique<ChannelFactory>(bus);
}

} // namespace vigine::channelfactory

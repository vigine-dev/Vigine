#include "vigine/impl/topicbus/topicbus.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "vigine/api/topicbus/factory.h"
#include "vigine/api/topicbus/topicid.h"
#include "vigine/api/messaging/imessage.h"
#include "vigine/api/messaging/imessagebus.h"
#include "vigine/api/messaging/imessagepayload.h"
#include "vigine/api/messaging/isubscriber.h"
#include "vigine/api/messaging/isubscriptiontoken.h"
#include "vigine/api/messaging/messagefilter.h"
#include "vigine/api/messaging/messagekind.h"
#include "vigine/api/messaging/routemode.h"
#include "vigine/api/messaging/payload/payloadtypeid.h"
#include "vigine/result.h"

namespace vigine::topicbus
{

namespace
{

// -----------------------------------------------------------------
// TopicMessage — concrete IMessage for a TopicPublish envelope.
// Private to this translation unit; never visible to callers.
// -----------------------------------------------------------------

class TopicPublishPayload final : public vigine::messaging::IMessagePayload
{
  public:
    explicit TopicPublishPayload(
        std::unique_ptr<vigine::messaging::IMessagePayload> inner) noexcept
        : _inner(std::move(inner))
    {
    }

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        if (_inner)
        {
            return _inner->typeId();
        }
        return vigine::payload::PayloadTypeId{};
    }

    [[nodiscard]] const vigine::messaging::IMessagePayload *inner() const noexcept
    {
        return _inner.get();
    }

  private:
    std::unique_ptr<vigine::messaging::IMessagePayload> _inner;
};

class TopicPublishMessage final : public vigine::messaging::IMessage
{
  public:
    TopicPublishMessage(TopicId                          topic,
                        std::unique_ptr<TopicPublishPayload> payload)
        : _topic(topic)
        , _payload(std::move(payload))
        , _scheduledFor(std::chrono::steady_clock::now())
    {
    }

    [[nodiscard]] vigine::messaging::MessageKind kind() const noexcept override
    {
        return vigine::messaging::MessageKind::TopicPublish;
    }

    [[nodiscard]] vigine::payload::PayloadTypeId payloadTypeId() const noexcept override
    {
        // Smuggle the topic id into the payload-type-id slot so the bus's
        // per-subscriber filter (which matches on PayloadTypeId) can route
        // publishes to the right subscriber. The inner user payload's real
        // type id is still reachable via payload()->typeId() through the
        // TopicPublishPayload wrapper.
        return vigine::payload::PayloadTypeId{_topic.value};
    }

    [[nodiscard]] const vigine::messaging::IMessagePayload *payload() const noexcept override
    {
        return _payload.get();
    }

    [[nodiscard]] const vigine::messaging::AbstractMessageTarget *
        target() const noexcept override
    {
        return nullptr; // FanOut: no specific target
    }

    [[nodiscard]] vigine::messaging::RouteMode routeMode() const noexcept override
    {
        return vigine::messaging::RouteMode::FanOut;
    }

    [[nodiscard]] vigine::messaging::CorrelationId correlationId() const noexcept override
    {
        return vigine::messaging::CorrelationId{};
    }

    [[nodiscard]] std::chrono::steady_clock::time_point
        scheduledFor() const noexcept override
    {
        return _scheduledFor;
    }

    [[nodiscard]] TopicId topic() const noexcept { return _topic; }

  private:
    TopicId                              _topic;
    std::unique_ptr<TopicPublishPayload> _payload;
    std::chrono::steady_clock::time_point _scheduledFor;
};

// -----------------------------------------------------------------
// Topic hash: FNV-1a 32-bit over the name bytes.
// Zero is never a valid id (reserved as sentinel).
// -----------------------------------------------------------------

[[nodiscard]] constexpr std::uint32_t fnv1a32(std::string_view s) noexcept
{
    std::uint32_t h = 0x811c9dc5u;
    for (unsigned char c : s)
    {
        h ^= c;
        h *= 0x01000193u;
    }
    return h;
}

[[nodiscard]] TopicId hashToId(std::string_view name, std::uint32_t salt) noexcept
{
    // Combine name hash with salt to resolve collisions.
    std::uint32_t v = fnv1a32(name) ^ (salt * 2654435761u);
    // Ensure non-zero.
    if (v == 0)
    {
        v = 1;
    }
    return TopicId{v};
}

} // namespace

// -----------------------------------------------------------------
// TopicBus::Impl
// -----------------------------------------------------------------

struct TopicBus::Impl
{
    // Topic registry: name -> TopicId (stable after creation).
    // Collisions: if hashToId produces a value already assigned to a
    // different name, we increment the salt until we find a free slot.
    mutable std::shared_mutex                       registryMutex;
    std::unordered_map<std::string, TopicId>        nameToId;
    std::unordered_map<std::uint32_t, std::string>  idToName;

    // Per-topic subscription tokens owned by the facade.
    // Key: TopicId::value.
    std::unordered_map<std::uint64_t,
        std::vector<std::unique_ptr<vigine::messaging::ISubscriptionToken>>> tokens;

    std::atomic<bool> shutdown{false};

    explicit Impl() = default;

    // Assign or retrieve a TopicId for name; must be called under write lock.
    [[nodiscard]] TopicId getOrCreate(std::string_view name)
    {
        // Check existing.
        auto it = nameToId.find(std::string{name});
        if (it != nameToId.end())
        {
            return it->second;
        }

        // Assign new id; resolve collision with salt.
        std::uint32_t salt = 0;
        TopicId id;
        do
        {
            id = hashToId(name, salt);
            auto dup = idToName.find(id.value);
            if (dup == idToName.end())
            {
                break; // slot free
            }
            if (dup->second == name)
            {
                // Same name already registered (race — shouldn't happen
                // under exclusive lock).
                return id;
            }
            ++salt;
        } while (true);

        std::string nameStr{name};
        nameToId[nameStr] = id;
        idToName[id.value] = std::move(nameStr);
        return id;
    }
};

// -----------------------------------------------------------------
// TopicBus
// -----------------------------------------------------------------

TopicBus::TopicBus(vigine::messaging::IMessageBus &bus)
    : AbstractTopicBus{bus}
    , _impl(std::make_unique<Impl>())
{
}

TopicBus::~TopicBus()
{
    shutdown();
}

TopicId TopicBus::createTopic(std::string_view name)
{
    if (name.empty())
    {
        return TopicId{};
    }

    std::unique_lock lock(_impl->registryMutex);
    return _impl->getOrCreate(name);
}

std::optional<TopicId> TopicBus::topicByName(std::string_view name) const
{
    if (name.empty())
    {
        return std::nullopt;
    }

    std::shared_lock lock(_impl->registryMutex);
    auto it = _impl->nameToId.find(std::string{name});
    if (it == _impl->nameToId.end())
    {
        return std::nullopt;
    }
    return it->second;
}

vigine::Result TopicBus::publish(
    TopicId                                            topic,
    std::unique_ptr<vigine::messaging::IMessagePayload> payload)
{
    if (!topic.valid())
    {
        return vigine::Result{vigine::Result::Code::Error, "invalid topic id"};
    }

    if (_impl->shutdown.load(std::memory_order_acquire))
    {
        return vigine::Result{vigine::Result::Code::Error, "topic bus shut down"};
    }

    auto wrapped  = std::make_unique<TopicPublishPayload>(std::move(payload));
    auto msg      = std::make_unique<TopicPublishMessage>(topic, std::move(wrapped));

    return bus().post(std::move(msg));
}

std::unique_ptr<vigine::messaging::ISubscriptionToken>
TopicBus::subscribe(TopicId topic, vigine::messaging::ISubscriber *subscriber)
{
    if (!subscriber)
    {
        return nullptr;
    }

    // Lazily create the topic if needed so subscribers can register before
    // the first publish (matches edge-case spec).
    if (!topic.valid())
    {
        return nullptr;
    }

    vigine::messaging::MessageFilter filter{};
    filter.kind          = vigine::messaging::MessageKind::TopicPublish;
    filter.expectedRoute = vigine::messaging::RouteMode::FanOut;
    // Scope the subscription to this topic: TopicPublishMessage reports
    // PayloadTypeId{topic.value} in payloadTypeId(), so the underlying bus
    // delivers only publishes that match this subscriber's topic.
    filter.typeId        = vigine::payload::PayloadTypeId{topic.value};

    return bus().subscribe(filter, subscriber);
}

vigine::Result TopicBus::shutdown()
{
    bool expected = false;
    if (!_impl->shutdown.compare_exchange_strong(
            expected, true,
            std::memory_order_acq_rel,
            std::memory_order_acquire))
    {
        return vigine::Result{vigine::Result::Code::Success};
    }

    // Release all subscription tokens owned by this facade.
    std::unique_lock lock(_impl->registryMutex);
    _impl->tokens.clear();

    return vigine::Result{vigine::Result::Code::Success};
}

// -----------------------------------------------------------------
// Factory
// -----------------------------------------------------------------

std::unique_ptr<ITopicBus>
createTopicBus(vigine::messaging::IMessageBus &bus)
{
    return std::make_unique<TopicBus>(bus);
}

} // namespace vigine::topicbus

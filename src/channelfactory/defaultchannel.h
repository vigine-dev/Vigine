#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>

#include "vigine/api/channelfactory/channelkind.h"
#include "vigine/api/channelfactory/ichannel.h"
#include "vigine/messaging/imessagepayload.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/result.h"

namespace vigine::channelfactory
{

/**
 * @brief Concrete channel: mutex + two condition variables + bounded FIFO.
 *
 * @ref DefaultChannel is the internal implementation of @ref IChannel. It is
 * never exposed in the public include tree; callers receive an @ref IChannel
 * pointer from @ref ChannelFactory::create.
 *
 * Implementation notes (v1):
 *   - NOT lock-free in v1 (documented here; lock-free is Q-FC3, deferred to
 *     v1.1).
 *   - Two condition variables: @c _notFull (wakes senders) and @c _notEmpty
 *     (wakes receivers) keep producer and consumer wake-ups independent.
 *   - @ref ChannelKind::Unbounded channels use the same path but skip the
 *     @c _notFull wait and impose no capacity cap.
 *   - A @c timeoutMs of -1 means "block indefinitely".
 *
 * Thread-safety: every entry point is safe to call from any thread.
 *
 * Invariants:
 *   - INV-11: no graph types appear in this header.
 *   - All data members are private (strict encapsulation).
 */
class DefaultChannel final : public IChannel
{
  public:
    /**
     * @brief Constructs a channel of @p kind with @p capacity and expected
     *        payload type @p expectedTypeId.
     *
     * Pre-conditions (enforced by @ref ChannelFactory::create before
     * constructing this object):
     *   - Bounded  => capacity >= 1.
     *   - Unbounded => capacity == 0.
     */
    DefaultChannel(ChannelKind                    kind,
                   std::size_t                    capacity,
                   vigine::payload::PayloadTypeId expectedTypeId);

    ~DefaultChannel() override;

    // IChannel
    [[nodiscard]] vigine::Result
        send(std::unique_ptr<vigine::messaging::IMessagePayload> payload,
             int timeoutMs) override;

    [[nodiscard]] bool
        trySend(std::unique_ptr<vigine::messaging::IMessagePayload> &payload) override;

    [[nodiscard]] vigine::Result
        receive(std::unique_ptr<vigine::messaging::IMessagePayload> &out,
                int timeoutMs) override;

    [[nodiscard]] bool
        tryReceive(std::unique_ptr<vigine::messaging::IMessagePayload> &out) override;

    void close() override;

    [[nodiscard]] bool         isClosed() const override;
    [[nodiscard]] std::size_t  size()     const override;

    DefaultChannel(const DefaultChannel &)            = delete;
    DefaultChannel &operator=(const DefaultChannel &) = delete;
    DefaultChannel(DefaultChannel &&)                 = delete;
    DefaultChannel &operator=(DefaultChannel &&)      = delete;

  private:
    ChannelKind                    _kind;
    std::size_t                    _capacity;
    vigine::payload::PayloadTypeId _expectedTypeId;

    mutable std::mutex              _mutex;
    std::condition_variable         _notFull;
    std::condition_variable         _notEmpty;
    std::queue<std::unique_ptr<vigine::messaging::IMessagePayload>> _queue;
    std::atomic<bool>               _closed{false};
};

} // namespace vigine::channelfactory

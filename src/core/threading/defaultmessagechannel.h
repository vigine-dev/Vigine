#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>

#include "vigine/result.h"
#include "vigine/core/threading/imessagechannel.h"

namespace vigine::core::threading
{
/**
 * @brief Default @ref IMessageChannel implementation — a bounded FIFO
 *        queue on @c std::mutex + two @c std::condition_variable
 *        instances (not-full, not-empty).
 *
 * Separate condition variables for the two waiter populations avoid the
 * classic "notify_all wakes a producer to find nothing changed" problem
 * of single-cv implementations: a @ref send increment wakes exactly
 * receivers, and a @ref receive decrement wakes exactly producers.
 *
 * @ref close flips an internal flag and broadcasts on both condition
 * variables so every blocked waiter re-evaluates its predicate. After
 * close, producers always fail fast; receivers continue draining until
 * the queue is empty and then fail fast too. The destructor calls
 * @ref close internally so outstanding waiters never hang on dtor.
 *
 * State is strictly private (INV — strict encapsulation).
 */
class DefaultMessageChannel final : public IMessageChannel
{
  public:
    explicit DefaultMessageChannel(std::size_t capacity) noexcept;
    ~DefaultMessageChannel() override;

    [[nodiscard]] Result
        send(Message message, std::chrono::milliseconds timeout) override;

    [[nodiscard]] bool trySend(Message &message) override;

    [[nodiscard]] Result
        receive(Message &out, std::chrono::milliseconds timeout) override;

    [[nodiscard]] bool tryReceive(Message &out) override;

    void close() override;

    [[nodiscard]] bool isClosed() const override;

    [[nodiscard]] std::size_t size() const override;

    [[nodiscard]] std::size_t capacity() const override;

  private:
    mutable std::mutex      _mutex;
    std::condition_variable _notFull;
    std::condition_variable _notEmpty;
    std::deque<Message>     _queue;
    std::size_t             _capacity;
    bool                    _closed;
};

} // namespace vigine::core::threading

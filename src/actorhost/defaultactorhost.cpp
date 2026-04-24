#include "vigine/actorhost/defaultactorhost.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "vigine/actorhost/iactor.h"
#include "vigine/actorhost/iactormailbox.h"
#include "vigine/messaging/imessage.h"
#include "vigine/messaging/imessagebus.h"
#include "vigine/messaging/messagekind.h"
#include "vigine/messaging/routemode.h"
#include "vigine/result.h"
#include "vigine/core/threading/itaskhandle.h"
#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/core/threading/irunnable.h"
#include "vigine/core/threading/namedthreadid.h"

namespace vigine::actorhost
{

// ---------------------------------------------------------------------------
// ActorMailQueue — mutex-protected FIFO of IMessage unique_ptrs.
// Using a plain std::queue so we can push polymorphic messages without
// serialising to bytes (IMessageChannel only carries byte buffers).
// ---------------------------------------------------------------------------

struct ActorMailQueue
{
    std::mutex                                         mtx;
    std::condition_variable                            cv;
    std::queue<std::unique_ptr<vigine::messaging::IMessage>> items;
    bool                                               closed{false};

    // Returns false if the queue is closed and empty (drain complete).
    bool push(std::unique_ptr<vigine::messaging::IMessage> msg)
    {
        std::lock_guard<std::mutex> lk(mtx);
        if (closed)
        {
            return false;
        }
        items.push(std::move(msg));
        cv.notify_one();
        return true;
    }

    // Blocks until an item is available or the queue is drained+closed.
    // Returns nullptr when the loop should exit.
    std::unique_ptr<vigine::messaging::IMessage> pop()
    {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [this]{ return !items.empty() || closed; });
        if (items.empty())
        {
            return nullptr; // closed and drained
        }
        auto msg = std::move(items.front());
        items.pop();
        return msg;
    }

    void close()
    {
        std::lock_guard<std::mutex> lk(mtx);
        closed = true;
        cv.notify_all();
    }
};

// ---------------------------------------------------------------------------
// ActorEntry — per-actor state kept in the host registry.
// ---------------------------------------------------------------------------

struct ActorEntry
{
    ActorId                                   id;
    std::unique_ptr<IActor>                   actor;
    std::shared_ptr<ActorMailQueue>           queue;
    vigine::core::threading::NamedThreadId          namedThread;
    std::unique_ptr<vigine::core::threading::ITaskHandle> taskHandle;
    std::atomic<bool>                         stopped{false};
};

// ---------------------------------------------------------------------------
// MailboxRunnable — the drain loop executed on the actor's named thread.
// ---------------------------------------------------------------------------

class MailboxRunnable final : public vigine::core::threading::IRunnable
{
  public:
    MailboxRunnable(std::shared_ptr<ActorMailQueue> queue,
                    IActor                         *actor)
        : _queue(std::move(queue))
        , _actor(actor)
    {
    }

    [[nodiscard]] vigine::Result run() override
    {
        // onStart — non-fatal; if it throws, log and proceed into the
        // receive loop so the actor still drains messages.
        try
        {
            _actor->onStart();
        }
        catch (const std::exception &ex)
        {
            std::fprintf(stderr,
                         "[vigine::actorhost] IActor::onStart threw: %s\n",
                         ex.what());
        }
        catch (...)
        {
            std::fprintf(stderr,
                         "[vigine::actorhost] IActor::onStart threw a "
                         "non-std::exception object\n");
        }

        // Drain loop — sequential; one message at a time. Exceptions
        // are caught + logged; the actor continues receiving (v1
        // semantics: no supervised restart). The comment above the
        // prior catch blocks claimed "crash logged" but no logging
        // ever ran — the catch-swallow was silent. Emit to stderr
        // so the next hang / missed delivery is traceable back to
        // the thrower. A proper logging hook replaces this when the
        // engine standardises one.
        while (true)
        {
            auto msg = _queue->pop();
            if (!msg)
            {
                // Queue closed and drained.
                break;
            }
            try
            {
                _actor->receive(*msg);
            }
            catch (const std::exception &ex)
            {
                std::fprintf(
                    stderr,
                    "[vigine::actorhost] IActor::receive threw: %s\n",
                    ex.what());
            }
            catch (...)
            {
                std::fprintf(stderr,
                             "[vigine::actorhost] IActor::receive threw a "
                             "non-std::exception object\n");
            }
        }

        // onStop — same log-and-continue policy.
        try
        {
            _actor->onStop();
        }
        catch (const std::exception &ex)
        {
            std::fprintf(stderr,
                         "[vigine::actorhost] IActor::onStop threw: %s\n",
                         ex.what());
        }
        catch (...)
        {
            std::fprintf(stderr,
                         "[vigine::actorhost] IActor::onStop threw a "
                         "non-std::exception object\n");
        }

        return vigine::Result{};
    }

    MailboxRunnable(const MailboxRunnable &)            = delete;
    MailboxRunnable &operator=(const MailboxRunnable &) = delete;
    MailboxRunnable(MailboxRunnable &&)                  = delete;
    MailboxRunnable &operator=(MailboxRunnable &&)       = delete;

  private:
    std::shared_ptr<ActorMailQueue> _queue;
    IActor                         *_actor;
};

// ---------------------------------------------------------------------------
// DefaultActorMailbox — RAII handle returned to the caller of spawn().
// ---------------------------------------------------------------------------

class DefaultActorMailbox final : public IActorMailbox
{
  public:
    DefaultActorMailbox(ActorId                         id,
                        std::shared_ptr<ActorMailQueue> queue,
                        std::function<void(ActorId)>    stopCallback)
        : _id(id)
        , _queue(std::move(queue))
        , _stopCallback(std::move(stopCallback))
    {
    }

    ~DefaultActorMailbox() override
    {
        stop();
    }

    [[nodiscard]] ActorId actorId() const noexcept override
    {
        return _id;
    }

    void stop() override
    {
        bool expected = false;
        if (_stopped.compare_exchange_strong(expected, true,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire))
        {
            _stopCallback(_id);
        }
    }

    DefaultActorMailbox(const DefaultActorMailbox &)            = delete;
    DefaultActorMailbox &operator=(const DefaultActorMailbox &) = delete;
    DefaultActorMailbox(DefaultActorMailbox &&)                  = delete;
    DefaultActorMailbox &operator=(DefaultActorMailbox &&)       = delete;

  private:
    ActorId                         _id;
    std::shared_ptr<ActorMailQueue> _queue;
    std::function<void(ActorId)>    _stopCallback;
    std::atomic<bool>               _stopped{false};
};

// ---------------------------------------------------------------------------
// DefaultActorHost::Impl
// ---------------------------------------------------------------------------

struct DefaultActorHost::Impl
{
    vigine::messaging::IMessageBus    &bus;
    vigine::core::threading::IThreadManager &threadManager;

    mutable std::mutex                              registryMutex;
    std::unordered_map<std::uint32_t, ActorEntry *> registry; // ActorId::value -> entry
    std::atomic<std::uint32_t>                      nextId{1};
    std::atomic<bool>                               shutdownFlag{false};

    explicit Impl(vigine::messaging::IMessageBus    &bus_,
                  vigine::core::threading::IThreadManager &tm_)
        : bus(bus_)
        , threadManager(tm_)
    {
    }

    // Allocates a fresh generational id.
    [[nodiscard]] ActorId allocateId() noexcept
    {
        return ActorId{nextId.fetch_add(1, std::memory_order_relaxed)};
    }
};

// ---------------------------------------------------------------------------
// DefaultActorHost
// ---------------------------------------------------------------------------

DefaultActorHost::DefaultActorHost(vigine::messaging::IMessageBus    &bus,
                                   vigine::core::threading::IThreadManager &threadManager)
    : AbstractActorHost(bus, threadManager)
    , _impl(std::make_unique<Impl>(bus, threadManager))
{
}

DefaultActorHost::~DefaultActorHost()
{
    shutdown();
}

std::unique_ptr<IActorMailbox> DefaultActorHost::spawn(std::unique_ptr<IActor> actor)
{
    if (!actor)
    {
        return nullptr;
    }

    if (_impl->shutdownFlag.load(std::memory_order_acquire))
    {
        return nullptr;
    }

    ActorId id = _impl->allocateId();

    auto queue = std::make_shared<ActorMailQueue>();

    // Build the entry on the heap; the registry holds a raw owning pointer so
    // we can hand out a stable IActor* to the runnable without dangling.
    auto *entry = new ActorEntry{};
    entry->id     = id;
    entry->actor  = std::move(actor);
    entry->queue  = queue;
    entry->stopped.store(false, std::memory_order_relaxed);

    // Register a named thread for this actor so messages are serialised.
    std::string threadName = "actor-" + std::to_string(id.value);
    entry->namedThread = _impl->threadManager.registerNamedThread(threadName);

    // Schedule the drain runnable on the actor's named thread.
    auto runnable = std::make_unique<MailboxRunnable>(queue, entry->actor.get());
    entry->taskHandle = _impl->threadManager.scheduleOnNamed(
        std::move(runnable), entry->namedThread);

    {
        std::lock_guard<std::mutex> lk(_impl->registryMutex);
        _impl->registry.emplace(id.value, entry);
    }

    // Build the stop callback: closes the queue and waits for the drain loop.
    auto stopCallback = [this](ActorId aid)
    {
        ActorEntry *e = nullptr;
        {
            std::lock_guard<std::mutex> lk(_impl->registryMutex);
            auto it = _impl->registry.find(aid.value);
            if (it == _impl->registry.end())
            {
                return;
            }
            e = it->second;
        }

        if (!e)
        {
            return;
        }

        bool expected = false;
        if (!e->stopped.compare_exchange_strong(expected, true,
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire))
        {
            return; // already stopped
        }

        // Close the queue so the drain loop exits after draining remaining messages.
        e->queue->close();

        // Wait for the drain loop (IRunnable::run) to complete.
        if (e->taskHandle)
        {
            (void)e->taskHandle->wait();
        }

        // Unregister the named thread.
        _impl->threadManager.unregisterNamedThread(e->namedThread);

        // Remove from registry and release.
        {
            std::lock_guard<std::mutex> lk(_impl->registryMutex);
            _impl->registry.erase(aid.value);
        }
        delete e;
    };

    return std::make_unique<DefaultActorMailbox>(id, queue, std::move(stopCallback));
}

vigine::Result DefaultActorHost::tell(
    ActorId                                       id,
    std::unique_ptr<vigine::messaging::IMessage>  message)
{
    if (!id.valid())
    {
        return vigine::Result{vigine::Result::Code::Error, "invalid actor id"};
    }

    // Reject a null message up front. The actor's receive() loop
    // dereferences every dequeued entry; pushing `nullptr` into the
    // mailbox would turn into a null deref inside the worker
    // thread with no obvious link back to the caller. Fail fast on
    // the caller's thread with a clear Result so the bad input is
    // visible at the point of entry.
    if (!message)
    {
        return vigine::Result{vigine::Result::Code::Error, "null message"};
    }

    if (_impl->shutdownFlag.load(std::memory_order_acquire))
    {
        return vigine::Result{vigine::Result::Code::Error, "actor host shut down"};
    }

    std::shared_ptr<ActorMailQueue> queue;
    {
        std::lock_guard<std::mutex> lk(_impl->registryMutex);
        auto it = _impl->registry.find(id.value);
        if (it == _impl->registry.end())
        {
            return vigine::Result{vigine::Result::Code::Error, "actor not found or stopped"};
        }
        queue = it->second->queue;
    }

    if (!queue->push(std::move(message)))
    {
        return vigine::Result{vigine::Result::Code::Error, "actor mailbox closed"};
    }

    return vigine::Result{};
}

void DefaultActorHost::stop(ActorId id)
{
    if (!id.valid())
    {
        return;
    }

    ActorEntry *e = nullptr;
    {
        std::lock_guard<std::mutex> lk(_impl->registryMutex);
        auto it = _impl->registry.find(id.value);
        if (it == _impl->registry.end())
        {
            return;
        }
        e = it->second;
    }

    if (!e)
    {
        return;
    }

    bool expected = false;
    if (!e->stopped.compare_exchange_strong(expected, true,
                                            std::memory_order_acq_rel,
                                            std::memory_order_acquire))
    {
        return;
    }

    e->queue->close();

    if (e->taskHandle)
    {
        (void)e->taskHandle->wait();
    }

    _impl->threadManager.unregisterNamedThread(e->namedThread);

    {
        std::lock_guard<std::mutex> lk(_impl->registryMutex);
        _impl->registry.erase(id.value);
    }
    delete e;
}

void DefaultActorHost::shutdown()
{
    bool already = _impl->shutdownFlag.exchange(true, std::memory_order_acq_rel);
    if (already)
    {
        return;
    }

    // Snapshot current entries.
    std::vector<ActorEntry *> entries;
    {
        std::lock_guard<std::mutex> lk(_impl->registryMutex);
        entries.reserve(_impl->registry.size());
        for (auto &[val, ptr] : _impl->registry)
        {
            entries.push_back(ptr);
        }
        _impl->registry.clear();
    }

    // Drain and join all actors.
    for (ActorEntry *e : entries)
    {
        bool expected = false;
        if (e->stopped.compare_exchange_strong(expected, true,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire))
        {
            e->queue->close();
            if (e->taskHandle)
            {
                (void)e->taskHandle->wait();
            }
            _impl->threadManager.unregisterNamedThread(e->namedThread);
        }
        delete e;
    }
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<IActorHost>
createActorHost(vigine::messaging::IMessageBus    &bus,
                vigine::core::threading::IThreadManager &threadManager)
{
    return std::make_unique<DefaultActorHost>(bus, threadManager);
}

} // namespace vigine::actorhost

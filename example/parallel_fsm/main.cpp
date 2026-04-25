// ---------------------------------------------------------------------------
// example-parallel-fsm
//
// Demonstrates two state machines, each pinned to its own named thread,
// driving a Ping <-> Pong exchange across the shared engine message bus.
//
// What the demo proves
// --------------------
//   * IThreadManager registers two named threads, "fsm_a" and "fsm_b".
//     Each FSM is bound to its named thread via
//     IStateMachine::bindToControllerThread; every sync mutation (the
//     transitions inside the cycle) runs on the bound thread, and the
//     debug affinity assert in AbstractStateMachine::checkThreadAffinity
//     stays silent.
//   * The two FSMs talk through one shared IMessageBus. Each FSM owns a
//     subscriber that, on receiving the peer's signal, posts a runnable
//     to its own named thread. That named-thread runnable performs the
//     sync transition() pair (active -> waiting -> active) and then
//     posts a fresh signal back to the peer.
//   * The cycle is bootstrapped by scheduling one Ping runnable on
//     "fsm_a"; the chain ends when both sides observe that the global
//     exchange counter has reached the configured target.
//   * The program prints the achieved/expected exchange count and exits
//     0 when both sides reached the target without errors.
//
// Why named threads + sync transition
// -----------------------------------
//   Subscribers fire on the publisher's thread (Shared bus policy in this
//   demo). Calling transition() directly inside onMessage() would violate
//   the controller-thread affinity once a binding is installed (the
//   AbstractStateMachine assert fires in Debug). The subscriber instead
//   schedules a runnable on the peer's named thread; the transition pair
//   then runs on the bound thread, which keeps the affinity gate happy.
//   The issue specifically excludes requestTransition / async drains for
//   this example -- the named-thread runnable performs sync transitions
//   exclusively.
// ---------------------------------------------------------------------------

#include "vigine/core/threading/factory.h"
#include "vigine/core/threading/irunnable.h"
#include "vigine/core/threading/itaskhandle.h"
#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/core/threading/namedthreadid.h"
#include "vigine/core/threading/threadaffinity.h"
#include "vigine/core/threading/threadmanagerconfig.h"
#include "vigine/messaging/busconfig.h"
#include "vigine/messaging/factory.h"
#include "vigine/messaging/imessage.h"
#include "vigine/messaging/imessagebus.h"
#include "vigine/messaging/imessagepayload.h"
#include "vigine/messaging/isubscriber.h"
#include "vigine/messaging/isubscriptiontoken.h"
#include "vigine/messaging/messagefilter.h"
#include "vigine/messaging/messagekind.h"
#include "vigine/messaging/routemode.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/result.h"
#include "vigine/statemachine/factory.h"
#include "vigine/statemachine/istatemachine.h"
#include "vigine/statemachine/stateid.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>

namespace
{

// Picked above the example-window reservation (0x20101 / 0x20102) and
// the threaded_bus example reservation (0x90001) so the demo cannot
// collide with any payload registered elsewhere in the engine.
constexpr vigine::payload::PayloadTypeId kPingPayloadTypeId{0x90101u};
constexpr vigine::payload::PayloadTypeId kPongPayloadTypeId{0x90102u};

// Total number of exchanges the demo aims to perform. One exchange =
// one signal travelling from one FSM to the other.
constexpr int kTargetExchanges = 100;

// Wall-clock safety belt: even if the cycle stalls for any reason the
// main thread will not block forever. The expected wall-clock time for
// kTargetExchanges = 100 is well below this on any modern machine.
constexpr auto kRunDeadline = std::chrono::seconds{10};

// ---------------------------------------------------------------------------
// Payload + envelope helpers (one envelope class per direction so the
// MessageFilter on the receiver side can route by typeId without inspecting
// the payload itself).
// ---------------------------------------------------------------------------

class PingPayload final : public vigine::messaging::IMessagePayload
{
  public:
    PingPayload() noexcept = default;

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        return kPingPayloadTypeId;
    }
};

class PongPayload final : public vigine::messaging::IMessagePayload
{
  public:
    PongPayload() noexcept = default;

    [[nodiscard]] vigine::payload::PayloadTypeId typeId() const noexcept override
    {
        return kPongPayloadTypeId;
    }
};

template <vigine::payload::PayloadTypeId Type, typename Payload>
class TypedMessage final : public vigine::messaging::IMessage
{
  public:
    TypedMessage() : _payload(std::make_unique<Payload>()) {}

    [[nodiscard]] vigine::messaging::MessageKind kind() const noexcept override
    {
        return vigine::messaging::MessageKind::Signal;
    }

    [[nodiscard]] vigine::payload::PayloadTypeId
        payloadTypeId() const noexcept override
    {
        return Type;
    }

    [[nodiscard]] const vigine::messaging::IMessagePayload *
        payload() const noexcept override
    {
        return _payload.get();
    }

    [[nodiscard]] const vigine::messaging::AbstractMessageTarget *
        target() const noexcept override
    {
        return nullptr;
    }

    [[nodiscard]] vigine::messaging::RouteMode routeMode() const noexcept override
    {
        return vigine::messaging::RouteMode::FirstMatch;
    }

    [[nodiscard]] vigine::messaging::CorrelationId
        correlationId() const noexcept override
    {
        return vigine::messaging::CorrelationId{};
    }

    [[nodiscard]] std::chrono::steady_clock::time_point
        scheduledFor() const noexcept override
    {
        return std::chrono::steady_clock::time_point{};
    }

  private:
    std::unique_ptr<Payload> _payload;
};

using PingMessage = TypedMessage<kPingPayloadTypeId, PingPayload>;
using PongMessage = TypedMessage<kPongPayloadTypeId, PongPayload>;

// ---------------------------------------------------------------------------
// Captures the std::thread::id of a named thread on first execution and
// signals the main thread that the binding can be installed.
// ---------------------------------------------------------------------------

class CaptureThreadIdRunnable final : public vigine::core::threading::IRunnable
{
  public:
    CaptureThreadIdRunnable(std::atomic<std::thread::id> &slot,
                            std::atomic<bool>            &captured) noexcept
        : _slot(slot), _captured(captured)
    {
    }

    [[nodiscard]] vigine::Result run() override
    {
        _slot.store(std::this_thread::get_id(), std::memory_order_release);
        _captured.store(true, std::memory_order_release);
        return vigine::Result{};
    }

  private:
    std::atomic<std::thread::id> &_slot;
    std::atomic<bool>            &_captured;
};

// ---------------------------------------------------------------------------
// FsmContext groups everything one side of the cycle needs.
//
// Each side exposes a single entry point: schedule one TickRunnable on
// its named thread. The runnable performs the active -> waiting -> active
// transition pair, increments the exchange counter, posts a signal on
// the shared bus addressed at the peer, and (if the target count has
// not yet been reached) leaves the next tick to the peer's response.
// ---------------------------------------------------------------------------

struct FsmContext
{
    vigine::statemachine::IStateMachine *fsm{nullptr};
    vigine::statemachine::StateId        active{};
    vigine::statemachine::StateId        waiting{};
};

// Per-side runnable that runs the actual transition pair on the named
// thread. The constructor captures everything by reference because the
// referenced objects (FSMs, bus, counters) outlive every scheduled
// runnable: main() joins both sides before letting any of them go out
// of scope.
template <typename ResponseMessage>
class TickRunnable final : public vigine::core::threading::IRunnable
{
  public:
    TickRunnable(FsmContext                  &self,
                 vigine::messaging::IMessageBus &bus,
                 std::atomic<int>            &exchanges,
                 int                          target) noexcept
        : _self(self), _bus(bus), _exchanges(exchanges), _target(target)
    {
    }

    [[nodiscard]] vigine::Result run() override
    {
        // Sync transition pair: active -> waiting -> active. The
        // bound-controller-thread invariant holds because this runnable
        // is queued through scheduleOnNamed onto the FSM's controller
        // thread.
        if (const vigine::Result r = _self.fsm->transition(_self.waiting);
            r.isError())
        {
            return r;
        }
        if (const vigine::Result r = _self.fsm->transition(_self.active);
            r.isError())
        {
            return r;
        }

        // Bump the global exchange counter before deciding whether to
        // post the response. fetch_add returns the old (pre-increment)
        // value; if that value is already >= _target the target was
        // reached by a different runnable -- do not post.
        const int prior = _exchanges.fetch_add(1, std::memory_order_acq_rel);
        if (prior >= _target)
        {
            // Already at or past target before we even got scheduled --
            // do nothing further. Reaching this branch means the peer
            // side already finished while this runnable sat on the queue.
            return vigine::Result{};
        }

        // Post the response signal so the peer can run its tick.
        const vigine::Result postResult =
            _bus.post(std::make_unique<ResponseMessage>());
        if (postResult.isError())
        {
            return postResult;
        }

        return vigine::Result{};
    }

  private:
    FsmContext                  &_self;
    vigine::messaging::IMessageBus &_bus;
    std::atomic<int>            &_exchanges;
    int                          _target;
};

using PingTickRunnable = TickRunnable<PongMessage>; // FSM A receives ping, replies with pong
using PongTickRunnable = TickRunnable<PingMessage>; // FSM B receives pong, replies with ping

// ---------------------------------------------------------------------------
// Subscriber that, on receiving its kind of message, schedules the
// peer's TickRunnable on the peer's named thread.
//
// The subscriber runs synchronously on the publisher's thread (Shared
// bus policy). It must NOT call transition() itself: the FSM is bound
// to the named thread and a transition from the wrong thread fires the
// affinity assert in Debug. Scheduling the tick runnable hands the
// transition over to the controller thread, which keeps the contract.
// ---------------------------------------------------------------------------

template <typename TickRunnableT>
class TickSubscriber final : public vigine::messaging::ISubscriber
{
  public:
    TickSubscriber(vigine::core::threading::IThreadManager &tm,
                   vigine::core::threading::NamedThreadId   target,
                   FsmContext                              &ownCtx,
                   vigine::messaging::IMessageBus          &bus,
                   std::atomic<int>                        &exchanges,
                   int                                      targetCount) noexcept
        : _tm(tm)
        , _target(target)
        , _ownCtx(ownCtx)
        , _bus(bus)
        , _exchanges(exchanges)
        , _targetCount(targetCount)
    {
    }

    [[nodiscard]] vigine::messaging::DispatchResult
        onMessage(const vigine::messaging::IMessage & /*message*/) override
    {
        // Stop scheduling more work once the cycle has hit its target.
        // The check is best-effort: the counter may have been bumped on
        // another thread between this load and the schedule call. The
        // race is benign -- the TickRunnable performs the same check on
        // its own thread before posting back, so at most one stray
        // runnable runs after the target is reached.
        if (_exchanges.load(std::memory_order_acquire) >= _targetCount)
        {
            return vigine::messaging::DispatchResult::Handled;
        }

        auto runnable = std::make_unique<TickRunnableT>(
            _ownCtx, _bus, _exchanges, _targetCount);
        auto handle =
            _tm.scheduleOnNamed(std::move(runnable), _target);
        // We deliberately drop the handle: the chain is self-clocking
        // -- the next tick will be scheduled by the peer once it
        // observes our reply on the bus -- and main() drains the cycle
        // by spinning on the exchange counter and then unregistering
        // both named threads (which joins their queues).
        (void)handle;

        return vigine::messaging::DispatchResult::Handled;
    }

  private:
    vigine::core::threading::IThreadManager  &_tm;
    vigine::core::threading::NamedThreadId    _target;
    FsmContext                               &_ownCtx;
    vigine::messaging::IMessageBus           &_bus;
    std::atomic<int>                         &_exchanges;
    int                                       _targetCount;
};

using PingSubscriber = TickSubscriber<PingTickRunnable>; // routes ping -> A's named thread
using PongSubscriber = TickSubscriber<PongTickRunnable>; // routes pong -> B's named thread

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

[[nodiscard]] bool captureNamedThreadId(
    vigine::core::threading::IThreadManager &tm,
    vigine::core::threading::NamedThreadId   id,
    std::atomic<std::thread::id>            &slot)
{
    std::atomic<bool> captured{false};
    auto runnable =
        std::make_unique<CaptureThreadIdRunnable>(slot, captured);
    auto handle = tm.scheduleOnNamed(std::move(runnable), id);
    if (!handle)
    {
        return false;
    }
    const vigine::Result r = handle->wait();
    return r.isSuccess() &&
           captured.load(std::memory_order_acquire);
}

[[nodiscard]] bool buildFsm(
    vigine::statemachine::IStateMachine &fsm,
    FsmContext                          &ctx)
{
    const auto active  = fsm.addState();
    const auto waiting = fsm.addState();
    if (!active.valid() || !waiting.valid())
    {
        return false;
    }
    if (fsm.setInitial(active).isError())
    {
        return false;
    }
    ctx.fsm     = &fsm;
    ctx.active  = active;
    ctx.waiting = waiting;
    return true;
}

} // namespace

int main()
{
    // ---- Threading substrate ------------------------------------------------
    auto threadManager =
        vigine::core::threading::createThreadManager(
            vigine::core::threading::ThreadManagerConfig{});
    if (!threadManager)
    {
        std::cerr << "createThreadManager failed\n";
        return 1;
    }

    // ---- Named threads, one per FSM -----------------------------------------
    const vigine::core::threading::NamedThreadId namedA =
        threadManager->registerNamedThread("fsm_a");
    const vigine::core::threading::NamedThreadId namedB =
        threadManager->registerNamedThread("fsm_b");
    if (!namedA.valid() || !namedB.valid())
    {
        std::cerr << "registerNamedThread failed for fsm_a or fsm_b\n";
        return 1;
    }

    // bindToControllerThread takes a std::thread::id, so we ask each
    // named thread to report its own id back before installing the
    // bindings. Without this round-trip we would have no way of pairing
    // the FSM with the right OS-level thread id.
    std::atomic<std::thread::id> threadIdA{};
    std::atomic<std::thread::id> threadIdB{};
    if (!captureNamedThreadId(*threadManager, namedA, threadIdA) ||
        !captureNamedThreadId(*threadManager, namedB, threadIdB))
    {
        std::cerr << "failed to capture named thread ids\n";
        return 1;
    }

    // ---- State machines -----------------------------------------------------
    auto fsmA = vigine::statemachine::createStateMachine();
    auto fsmB = vigine::statemachine::createStateMachine();
    if (!fsmA || !fsmB)
    {
        std::cerr << "createStateMachine returned null\n";
        return 1;
    }

    FsmContext ctxA{};
    FsmContext ctxB{};
    if (!buildFsm(*fsmA, ctxA) || !buildFsm(*fsmB, ctxB))
    {
        std::cerr << "FSM state registration failed\n";
        return 1;
    }

    // Install one-shot controller-thread bindings. After this point any
    // sync transition() call must run on the bound thread -- which is
    // why every transition lives inside the TickRunnable executed via
    // scheduleOnNamed.
    fsmA->bindToControllerThread(threadIdA.load(std::memory_order_acquire));
    fsmB->bindToControllerThread(threadIdB.load(std::memory_order_acquire));

    // ---- Shared bus + subscribers ------------------------------------------
    vigine::messaging::BusConfig busConfig{};
    busConfig.name         = "parallel-fsm-example";
    busConfig.priority     = vigine::messaging::BusPriority::Normal;
    busConfig.threading    = vigine::messaging::ThreadingPolicy::Shared;
    busConfig.capacity     = vigine::messaging::QueueCapacity{
        /*maxMessages=*/static_cast<std::size_t>(kTargetExchanges * 4),
        /*bounded=*/true};
    busConfig.backpressure = vigine::messaging::BackpressurePolicy::Block;

    auto bus =
        vigine::messaging::createMessageBus(busConfig, *threadManager);
    if (!bus)
    {
        std::cerr << "createMessageBus failed\n";
        return 1;
    }

    std::atomic<int> exchanges{0};

    // Ping arrives at A's subscriber, which schedules a ping-tick runnable
    // on A's named thread. Pong arrives at B's subscriber, which schedules
    // a pong-tick runnable on B's named thread. Each runnable transitions
    // the FSM and posts the response message.
    PingSubscriber subscriberA{*threadManager, namedA, ctxA, *bus,
                                exchanges, kTargetExchanges};
    PongSubscriber subscriberB{*threadManager, namedB, ctxB, *bus,
                                exchanges, kTargetExchanges};

    vigine::messaging::MessageFilter filterPing{};
    filterPing.kind   = vigine::messaging::MessageKind::Signal;
    filterPing.typeId = kPingPayloadTypeId;

    vigine::messaging::MessageFilter filterPong{};
    filterPong.kind   = vigine::messaging::MessageKind::Signal;
    filterPong.typeId = kPongPayloadTypeId;

    auto tokenA = bus->subscribe(filterPing, &subscriberA);
    auto tokenB = bus->subscribe(filterPong, &subscriberB);
    if (tokenA == nullptr || !tokenA->active() ||
        tokenB == nullptr || !tokenB->active())
    {
        std::cerr << "failed to subscribe ping/pong listeners\n";
        return 1;
    }

    // ---- Bootstrap the cycle ------------------------------------------------
    //
    // Schedule the very first tick on A. That runnable transitions A,
    // bumps the exchange counter to 1, and posts a pong message; B's
    // subscriber picks it up, schedules B's tick, which posts a ping;
    // A's subscriber picks it up, ..., until the counter reaches the
    // target.
    {
        auto firstTick = std::make_unique<PingTickRunnable>(
            ctxA, *bus, exchanges, kTargetExchanges);
        auto handle = threadManager->scheduleOnNamed(
            std::move(firstTick), namedA);
        if (!handle)
        {
            std::cerr << "failed to schedule the bootstrap tick\n";
            return 1;
        }
        const vigine::Result r = handle->wait();
        if (r.isError())
        {
            std::cerr << "bootstrap tick reported error: "
                      << r.message() << "\n";
            return 1;
        }
    }

    // ---- Drain ---------------------------------------------------------------
    //
    // Spin until the exchange counter reaches the target or the deadline
    // expires. The cycle is fully scheduler-driven from here -- main
    // does no more posting.
    const auto deadline = std::chrono::steady_clock::now() + kRunDeadline;
    while (exchanges.load(std::memory_order_acquire) < kTargetExchanges &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::yield();
    }

    const int finalCount = exchanges.load(std::memory_order_acquire);

    // Drop the subscription tokens before unregistering the named
    // threads so no stray onMessage call sneaks in after the threads
    // are gone.
    tokenA.reset();
    tokenB.reset();

    // Unregister both named threads -- this drains any in-flight
    // runnables before the threads exit. Doing it explicitly (instead
    // of leaving it to ~IThreadManager) makes the ordering visible.
    threadManager->unregisterNamedThread(namedA);
    threadManager->unregisterNamedThread(namedB);

    std::cout << "exchanges: " << finalCount << "/" << kTargetExchanges
              << "\n";

    const bool ok = finalCount >= kTargetExchanges;
    return ok ? 0 : 1;
}

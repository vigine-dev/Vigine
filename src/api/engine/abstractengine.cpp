#include "vigine/api/engine/abstractengine.h"

#include <chrono>
#include <thread>

#include "vigine/api/context/factory.h"
#include "vigine/api/context/icontext.h"
#include "vigine/api/statemachine/istatemachine.h"
#include "vigine/core/threading/ithreadmanager.h"

namespace vigine::engine
{

// The constructor initialiser list encodes the strict ctor order:
// step 1 builds the context aggregator (which internally builds the
// thread manager first, then the system bus, then the Level-1
// wrappers); step 2 captures the run-mode hint; the lifecycle flags
// default-initialise through their declared member default values.
AbstractEngine::AbstractEngine(const EngineConfig &config)
    : _context{context::createContext(config.context)}
    , _runMode{config.runMode}
{
}

// The destructor mirrors the ctor order in reverse: lifecycle state
// dies first (the atomic flags and sync primitives are trivially
// destructible; the mutex + CV release their internal state with no
// side effects), then _context dies last, cascading the documented
// reverse teardown through its own member declaration order (services
// registry, Level-1 wrappers, system bus, thread manager last).
//
// Before letting _context tear down we do three things explicitly:
//   1. Call _context->freeze() so any late post-back that slips
//      through observes a closed topology and bounces with
//      Result::Code::TopologyFrozen rather than attempting a mutation
//      on a half-dead aggregator.
//   2. Drain the FSM's queued transitions one last time on the
//      destruction thread when it is safe to do so (the FSM is
//      unbound, or its bound controller thread matches this one).
//      Best-effort per AD-G5: requestTransition calls posted after
//      this drain land on a queue that is about to be torn down with
//      the FSM, so they are silently dropped without surfacing any
//      Result. The thread-affinity guard avoids tripping the Debug
//      assert in checkThreadAffinity when the engine is destroyed
//      from a thread other than the one that drove run().
//   3. Drain the thread manager's main-thread queue one last time so
//      any runnable posted by a caller between createEngine() and
//      destruction (without a matching run()) is observed on the
//      destruction thread rather than being silently discarded by the
//      thread manager's own shutdown later in the teardown chain.
AbstractEngine::~AbstractEngine()
{
    if (_context)
    {
        _context->freeze();

        statemachine::IStateMachine &fsm  = _context->stateMachine();
        const std::thread::id        bound = fsm.controllerThread();
        if (bound == std::thread::id{} || bound == std::this_thread::get_id())
        {
            fsm.processQueuedTransitions();
        }

        _context->threadManager().runMainThreadPump();
    }
}

// ---------------------------------------------------------------------
// IEngine
// ---------------------------------------------------------------------

IContext &AbstractEngine::context()
{
    // The aggregator is built eagerly in the ctor and lives until the
    // dtor, so the dereference is always safe between construction
    // and destruction.
    return *_context;
}

Result AbstractEngine::run()
{
    // Single-shot guard: the first call through flips _runEntered
    // from false to true. Subsequent calls see the flag set and
    // return an error Result without mutating any state. The CAS uses
    // acq_rel so the winning thread observes every prior store
    // published by the loser (and vice versa for the read-side on
    // reject).
    bool expected = false;
    if (!_runEntered.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel, std::memory_order_acquire))
    {
        return Result{
            Result::Code::Error,
            "AbstractEngine::run: lifecycle is single-shot; build a new engine"};
    }

    // Freeze the topology before entering the pump so any post-run
    // registerService / createMessageBus call is rejected. Callers
    // that expect mutations to succeed must perform them between
    // createEngine() and run().
    _context->freeze();

    // Pin the engine-wide state machine to this thread per AD-G2: the
    // thread that drives the pump loop is the FSM's controller thread.
    // bindToControllerThread is one-shot, so this is safe only when no
    // earlier caller (e.g. a test that drove the FSM directly off the
    // shared context) has already bound the FSM to a different thread.
    // The contract on a double-bind is "Debug asserts; Release silently
    // keeps the original binding" — we honour both: in production the
    // engine owns the binding, while tests that bypass the engine
    // continue to bind manually without the engine clobbering them.
    statemachine::IStateMachine &fsm = _context->stateMachine();
    if (fsm.controllerThread() == std::thread::id{})
    {
        fsm.bindToControllerThread(std::this_thread::get_id());
    }

    // Publish the running flag with release semantics so an observer
    // that sees isRunning() == true also sees the freeze side effect.
    _running.store(true, std::memory_order_release);

    // Main-thread pump loop. Each iteration is the engine "tick" per
    // AD-G3: we drain queued FSM transitions on the controller thread
    // first (so requestTransition calls posted from worker threads are
    // applied before any main-thread runnable observes the new state),
    // then drain the thread manager's main-thread queue, then wait for
    // either a shutdown request or the pump tick timeout so a
    // shutdown() call is observed with bounded latency. The shutdown
    // flag is checked twice per tick -- once under the mutex (so a
    // concurrent shutdown + notify pair cannot be lost) and once
    // lock-free before each drain (so a shutdown that arrives during
    // the drain is observed at the next predicate check).
    core::threading::IThreadManager &tm = _context->threadManager();
    const auto tick = std::chrono::milliseconds{pumpTickMilliseconds()};

    while (!_shutdownRequested.load(std::memory_order_acquire))
    {
        // Drain queued FSM transitions on this (controller) thread.
        // processQueuedTransitions is single-pass and snapshot-swap;
        // requests posted during the drain land on the live queue and
        // are picked up on the next iteration, per the cooperative
        // no-reentry contract documented on
        // IStateMachine::processQueuedTransitions.
        fsm.processQueuedTransitions();

        // Drain any main-thread work posted since the last tick.
        // runMainThreadPump is permitted to run zero or more
        // runnables synchronously on the calling thread; it returns
        // once the queue is empty at the moment of observation.
        tm.runMainThreadPump();

        // Block until either shutdown is requested or the pump tick
        // elapses. The predicate uses acquire semantics so the wake
        // happens-before the shutdown side effect observable by the
        // loop body.
        std::unique_lock lock{_pumpMutex};
        _pumpCv.wait_for(lock, tick, [this]() noexcept {
            return _shutdownRequested.load(std::memory_order_acquire);
        });
    }

    // Final drain after shutdown is observed so requests posted between
    // "wait returns" and "loop exits" are applied before the FSM is
    // torn down. AD-G5 calls this the best-effort drain pass: every
    // requestTransition call already on the queue is honoured exactly
    // once; calls that race in after this point land on a queue that
    // is about to die with the context, so they are silently dropped
    // without surfacing any Result back to the producer.
    fsm.processQueuedTransitions();

    // Final drain of the thread manager's main-thread queue so any
    // post-back that arrived between "wait returns" and "loop exits"
    // is not stranded. The thread manager's own shutdown (driven by
    // the context's dtor) would discard late runnables anyway;
    // draining here gives deterministic semantics for tests that
    // shutdown from inside a main-thread runnable.
    tm.runMainThreadPump();

    _running.store(false, std::memory_order_release);
    return Result{};
}

void AbstractEngine::shutdown() noexcept
{
    // Flip the flag under the mutex so the wait-predicate consultation
    // on the main loop's side cannot miss the notify. std::lock_guard
    // gives the mutex RAII semantics; if notify_all threw (it does not
    // per the C++ spec, but defence in depth is cheap) the mutex
    // unlocks anyway. The flag uses release semantics so every prior
    // write on the calling thread happens-before any subsequent
    // acquire-load on the pump side.
    {
        std::lock_guard lock{_pumpMutex};
        _shutdownRequested.store(true, std::memory_order_release);
    }
    _pumpCv.notify_all();
}

bool AbstractEngine::isRunning() const noexcept
{
    return _running.load(std::memory_order_acquire);
}

// ---------------------------------------------------------------------
// Protected helpers
// ---------------------------------------------------------------------

unsigned AbstractEngine::pumpTickMilliseconds() const noexcept
{
    // 4ms is short enough that a shutdown from any thread is observed
    // with negligible perceived latency, yet long enough that an idle
    // engine does not saturate a CPU core. Tests override the value
    // if they need tighter bounds; production uses this default.
    return 4u;
}

RunMode AbstractEngine::runMode() const noexcept
{
    return _runMode;
}

} // namespace vigine::engine

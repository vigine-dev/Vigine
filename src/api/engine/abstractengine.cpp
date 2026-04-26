#include "vigine/api/engine/abstractengine.h"

#include <chrono>
#include <cstdio>
#include <thread>

#include "vigine/api/context/factory.h"
#include "vigine/api/context/icontext.h"
#include "vigine/api/statemachine/istatemachine.h"
#include "vigine/api/statemachine/stateid.h"
#include "vigine/api/taskflow/itaskflow.h"
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
    //
    // We also classify the binding outcome into a boolean
    // (fsmDrainSafe): when this thread is the FSM's controller (either
    // because we just bound it or because an earlier bind landed on
    // exactly this thread), draining the queued transitions on every
    // tick is safe. When the FSM is already bound to a different
    // thread (a test embedder or a host application that drove the
    // FSM directly off the shared context before calling run()),
    // calling processQueuedTransitions on this thread would trip the
    // Debug thread-affinity assert in checkThreadAffinity. In that
    // case we skip the FSM drain altogether for the rest of run() and
    // emit a one-shot warning on stderr so the silent-skip is visible
    // in test logs. The thread manager's main-thread pump remains
    // active so post-backs to the engine thread are still observed.
    statemachine::IStateMachine &fsm        = _context->stateMachine();
    const std::thread::id        bound      = fsm.controllerThread();
    const std::thread::id        selfId     = std::this_thread::get_id();
    bool                         fsmDrainSafe = true;
    if (bound == std::thread::id{})
    {
        fsm.bindToControllerThread(selfId);
    }
    else if (bound != selfId)
    {
        fsmDrainSafe = false;
        std::fprintf(stderr,
                     "[vigine::engine] FSM bound to non-engine controller "
                     "thread before run(); skipping FSM drains for this "
                     "run() invocation to preserve thread affinity\n");
    }

    // Publish the running flag with release semantics so an observer
    // that sees isRunning() == true also sees the freeze side effect.
    _running.store(true, std::memory_order_release);

    // Main-thread pump loop. Each iteration is the engine "tick" per
    // AD-G3: we (1) advance the TaskFlow bound to the FSM's current
    // state by exactly one step, (2) drain queued FSM transitions on
    // the controller thread (so requestTransition calls posted from
    // worker threads -- including those just posted by the task that
    // ran in step 1 -- are applied before the next state read), (3)
    // drain the thread manager's main-thread queue, then (4) wait for
    // either a shutdown request or the pump tick timeout so a
    // shutdown() call is observed with bounded latency. The shutdown
    // flag is checked twice per tick -- once under the mutex (so a
    // concurrent shutdown + notify pair cannot be lost) and once
    // lock-free before each drain (so a shutdown that arrives during
    // the drain is observed at the next predicate check).
    //
    // FSM-drive contract:
    //   The engine looks up the TaskFlow bound to the FSM's current
    //   state through IStateMachine::taskFlowFor(current()). When the
    //   lookup hits, the engine drives exactly one TaskFlow step per
    //   tick by calling runCurrentTask(). That call asks IContext for
    //   a fresh engine token (when a context has been wired into the
    //   flow via TaskFlow::setContext), binds it on the task via
    //   setApi, runs the task once, clears the binding, and lets the
    //   token go out of scope so any subscribeExpiration callbacks
    //   that fired during run() can finish their bookkeeping. When
    //   the lookup misses (no TaskFlow registered for the current
    //   state) or the bound flow has no further task to run
    //   (hasTasksToRun() == false), the tick falls through to the FSM
    //   drain + main-thread pump alone, matching the pre-FSM-drive
    //   behaviour for callers that drive the engine without a
    //   state-bound flow.
    //
    //   Engine-token state binding -- honest current state:
    //     This leaf wires the per-tick lookup through taskFlowFor but
    //     does NOT yet thread the FSM's current StateId into the token
    //     mint inside TaskFlow::runCurrentTask. The legacy
    //     TaskFlow::runCurrentTask path passes a sentinel-default
    //     vigine::statemachine::StateId{} into IContext::makeEngineToken
    //     (see src/impl/taskflow/taskflow.cpp). The legacy
    //     Context::makeEngineToken stub ignores its argument; the
    //     modern AbstractContext token factory tolerates the sentinel
    //     and threads it into the concrete token. So the engine-token
    //     a task observes per tick today is NOT bound to the FSM's
    //     real current state -- it carries the sentinel id. Wiring
    //     the real id through here requires either a runCurrentTask
    //     overload that accepts an externally minted token or a
    //     redesign of the legacy TaskFlow::setContext path; both are
    //     larger than the FSM-drive infrastructure scope of this leaf
    //     and are flagged for follow-up. The engine-token contract
    //     suite (scenario_21/22) and the legacy demo path continue to
    //     verify the token shape itself; this comment only documents
    //     that the state-id field threaded into that token is the
    //     sentinel, not fsm.current(), until the follow-up lands.
    //
    //   The FSM-drive step skips entirely when the FSM is bound to an
    //   alien thread (fsmDrainSafe == false) -- the caller asked the
    //   engine to stay out of the FSM's controller-thread contract,
    //   so we honour that and let them drive their own task pump
    //   externally. This keeps tests that bind the FSM directly off
    //   the shared context working without surprise tick-time mutations.
    core::threading::IThreadManager &tm = _context->threadManager();
    const auto tick = std::chrono::milliseconds{pumpTickMilliseconds()};

    while (!_shutdownRequested.load(std::memory_order_acquire))
    {
        // FSM-drive step: advance the TaskFlow bound to the current
        // state by one. The lookup is best-effort (a null result is
        // the explicit "no flow registered" signal); the per-tick
        // pump shape lets the FSM transition between ticks change
        // which flow is driven without the engine needing to track
        // any cross-tick state. Gated by fsmDrainSafe so an alien-
        // bound FSM does not get a controller-thread mutation here:
        // its embedder is responsible for driving its own flows.
        if (fsmDrainSafe)
        {
            const statemachine::StateId   currentState = fsm.current();
            vigine::taskflow::ITaskFlow *boundFlow    = fsm.taskFlowFor(currentState);
            if (boundFlow != nullptr && boundFlow->hasTasksToRun())
            {
                /*
                 * Wire the engine context into the bound flow so it
                 * can mint per-state IEngineTokens via
                 * IContext::makeEngineToken. The assignment is
                 * idempotent; the flow stores a non-owning back-pointer.
                 */
                boundFlow->setContext(_context.get());

                /*
                 * Drive the per-state token lifecycle: tell the flow
                 * which FSM state is currently active. The flow keeps
                 * the same token across ticks for as long as the state
                 * is active and only mints a fresh one (firing
                 * expiration callbacks on the prior token's subscribers)
                 * when the state genuinely changes between ticks.
                 */
                boundFlow->setActiveState(currentState);

                /*
                 * runCurrentTask handles the per-task setApi /
                 * setApi(nullptr) lifecycle on its own through its
                 * RAII guard; the engine just tells it to advance
                 * once. Any FSM transition requested by the task
                 * during run() lands on the FSM's request queue and
                 * is drained on the very next call below, so the
                 * next tick observes the new state.
                 */
                boundFlow->runCurrentTask();
            }
        }

        // Drain queued FSM transitions on this (controller) thread.
        // processQueuedTransitions is single-pass and snapshot-swap;
        // requests posted during the drain land on the live queue and
        // are picked up on the next iteration, per the cooperative
        // no-reentry contract documented on
        // IStateMachine::processQueuedTransitions. Gated by
        // fsmDrainSafe so an alien-bound FSM (tests / embedders) does
        // not trip the controller-thread assertion in checkThreadAffinity.
        if (fsmDrainSafe)
        {
            fsm.processQueuedTransitions();
        }

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
    // without surfacing any Result back to the producer. Gated by
    // fsmDrainSafe for the same reason as the in-loop drain: an
    // alien-bound FSM is the embedder's responsibility to drain.
    if (fsmDrainSafe)
    {
        fsm.processQueuedTransitions();
    }

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

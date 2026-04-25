// ---------------------------------------------------------------------------
// example-fanout-fsm
//
// Demonstrates that vigine::core::threading::parallelFor can fan out a
// range of work items across the IThreadManager pool and that each
// dispatched body can drive its own independent IStateMachine through a
// small state cycle without contending with the others.
//
// What the demo proves
// --------------------
//   * IThreadManager constructed with default config (poolSize derived
//     from std::thread::hardware_concurrency()).
//   * parallelFor splits a [0, kFsmCount) range into chunks sized so each
//     pool worker pulls roughly one chunk; the helper returns one
//     aggregated ITaskHandle whose wait() blocks until every chunk
//     settled, propagating the first chunk error if any.
//   * Each parallelFor body owns its own IStateMachine (created via
//     statemachine::createStateMachine) and walks it through a fixed
//     state cycle: Idle -> Working -> Done. Per-body FSMs are local to
//     the body, never bound to a controller thread, so the
//     AbstractStateMachine::checkThreadAffinity gate is intentionally
//     inactive and sync transition() runs from whichever pool worker
//     happens to pick the chunk up.
//   * After the aggregated handle waits successfully, main aggregates
//     the per-body completion bits via an atomic counter and asserts
//     ordering: every body that ran reports a successful Result and
//     leaves its FSM at the Done state.
//
// Output: "fanout completed: X/N FSMs reached final state".
// Exit code 0 only when X == N AND every parallelFor chunk reported
// success.
//
// Why each body owns its FSM (and not one shared FSM bound elsewhere)
// -------------------------------------------------------------------
//   parallel_fsm's pattern -- bindToControllerThread + scheduleOnNamed
//   -- exists to serialise transitions on a *shared* FSM that several
//   actors poke. The fanout demo wants the opposite: N independent
//   workflows running in parallel, with no shared FSM at all. Keeping
//   each FSM private to its parallelFor body means there is nothing
//   for the bodies to race on -- the per-FSM std::shared_mutex inside
//   the substrate is the only synchronisation point and it is only
//   ever taken by the body that owns the machine.
// ---------------------------------------------------------------------------

#include "vigine/core/threading/factory.h"
#include "vigine/core/threading/itaskhandle.h"
#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/core/threading/parallel_for.h"
#include "vigine/core/threading/threadaffinity.h"
#include "vigine/core/threading/threadmanagerconfig.h"
#include "vigine/result.h"
#include "vigine/statemachine/factory.h"
#include "vigine/statemachine/istatemachine.h"
#include "vigine/statemachine/stateid.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <vector>

namespace
{

// Number of independent FSMs to drive in parallel. Picked large enough
// that a typical pool (4-16 workers) gets several chunks, but small
// enough that the demo's wall-clock stays well under one second on any
// reasonable host.
constexpr std::size_t kFsmCount = 16;

// Wall-clock safety belt for the aggregated wait(). The fan-out body
// is purely local and trivially fast on every modern machine; five
// seconds is two orders of magnitude over the expected runtime.
constexpr auto kWaitTimeout = std::chrono::seconds{5};

// Per-body bit pattern used to record "this index completed its FSM
// cycle successfully". Indexed by the parallelFor index so the aggregate
// step can verify every body ran exactly once.
//
// std::atomic<bool> per slot keeps the recording lock-free; the
// surrounding std::vector is sized once before parallelFor dispatches
// any chunk, and the slots are never resized while bodies run.
struct CompletionSlot
{
    std::atomic<bool> done{false};
};

} // namespace

int main()
{
    // ---- Threading substrate ------------------------------------------------
    auto threadManager = vigine::core::threading::createThreadManager(
        vigine::core::threading::ThreadManagerConfig{});
    if (!threadManager)
    {
        std::cerr << "createThreadManager failed\n";
        return 1;
    }

    // ---- Per-FSM completion bookkeeping ------------------------------------
    //
    // Sized to kFsmCount so every parallelFor index has a stable slot.
    // The vector itself is not resized after construction, so it is safe
    // for concurrent writes from different bodies as long as each body
    // writes only to slots[index] (the parallelFor contract guarantees
    // each index is dispatched exactly once).
    std::vector<CompletionSlot> slots(kFsmCount);

    // ---- Fan out -----------------------------------------------------------
    //
    // Each body constructs its own IStateMachine, treats the auto-
    // provisioned default state as Idle, and registers Working and Done
    // states for the cycle. The body walks Idle -> Working -> Done and
    // records its completion bit. No body shares any state with any
    // other body.
    auto handle = vigine::core::threading::parallelFor(
        *threadManager,
        kFsmCount,
        [&slots](std::size_t index) {
            // Per-body FSM. Owned by this body via std::unique_ptr so
            // the machine is destroyed at body exit; the substrate
            // releases its mutex and its state topology cleanly when
            // the unique_ptr goes out of scope.
            auto fsm = vigine::statemachine::createStateMachine();
            if (!fsm)
            {
                // Leave the slot at the default-constructed `false` so
                // the aggregate step counts this index as not-completed
                // and main exits non-zero. parallelFor itself receives
                // a successful Result -- the aggregate is the source of
                // truth for the demo.
                return;
            }

            // Register the working and final states. The FSM auto-
            // provisions a default state in its constructor, which we
            // treat as the Idle slot. addState() always returns a valid
            // StateId, so no defensive guard is needed here.
            const vigine::statemachine::StateId working = fsm->addState();
            const vigine::statemachine::StateId done    = fsm->addState();

            // Walk the cycle. Each transition is sync; the FSM is not
            // bound to a controller thread, so checkThreadAffinity is
            // intentionally inactive. transition() returns an error
            // Result for stale ids -- treat any error as a body
            // failure, leaving the slot at false so the aggregate
            // counts it as not-completed.
            if (fsm->transition(working).isError())
            {
                return;
            }
            if (fsm->transition(done).isError())
            {
                return;
            }

            // Final-state assertion: the FSM must report `done` as the
            // current state after the last transition. If it does not,
            // do NOT mark the slot as completed -- the aggregate will
            // treat this body as a failure.
            if (fsm->current() != done)
            {
                return;
            }

            // Record the success bit. release-store pairs with the
            // acquire-load in main so the aggregate step sees a
            // consistent view of every per-slot write.
            slots[index].done.store(true, std::memory_order_release);
        });

    if (!handle)
    {
        std::cerr << "parallelFor returned a null handle\n";
        return 1;
    }

    // Bounded wait. waitFor returns the aggregated Result -- success
    // when every chunk completed without error, otherwise the first
    // failing chunk's error (timeout returns its own error message).
    const vigine::Result waitResult = handle->waitFor(
        std::chrono::duration_cast<std::chrono::milliseconds>(kWaitTimeout));
    if (waitResult.isError())
    {
        std::cerr << "parallelFor wait failed: " << waitResult.message()
                  << "\n";
        return 1;
    }

    // ---- Aggregate ---------------------------------------------------------
    //
    // Count slots that reported completion. The acquire-load pairs with
    // the release-store inside each body so every successful write is
    // visible here.
    std::size_t completed = 0;
    for (const auto &slot : slots)
    {
        if (slot.done.load(std::memory_order_acquire))
        {
            ++completed;
        }
    }

    std::cout << "fanout completed: " << completed << "/" << kFsmCount
              << " FSMs reached final state\n";

    return (completed == kFsmCount) ? 0 : 1;
}

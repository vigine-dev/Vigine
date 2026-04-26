// ---------------------------------------------------------------------------
// Scenario 17 -- FSM controller-thread affinity assertion.
//
// IStateMachine::bindToControllerThread installs a one-shot binding:
// the bound thread is the only thread allowed to call sync mutators
// (setInitial / transition / addChildState / processQueuedTransitions
// once a binding is in place). The Debug build enforces the binding
// via assert() inside AbstractStateMachine::checkThreadAffinity, fired
// at the entry of every gated mutator. Release intentionally drops the
// gate: the entire body is wrapped in #ifndef NDEBUG.
//
// What this scenario pins
// -----------------------
//   - A second-call rebind is rejected: the contract says the binding
//     is one-shot, so a follow-up bindToControllerThread must NOT
//     overwrite the original. We exercise that by reading
//     controllerThread() back and confirming the second call did not
//     change the bound id. This case is portable across Debug and
//     Release builds: the early-out path uses a CAS, not an assert,
//     so the observable effect (no overwrite) holds in both modes.
//
//   - A wrong-thread sync mutation aborts in Debug. We use
//     EXPECT_DEATH to spawn a child gtest process that calls
//     transition() from a thread that is NOT the bound controller;
//     the assert fires, the child aborts, and the parent observes
//     the death-test pass. The case is gated on a Debug build via
//     #ifndef NDEBUG so a Release run reports a single SUCCEED
//     stub instead of trying to assert on a binding the build
//     skipped wiring up.
//
// Both cases run on the EngineFixture so the FSM is reached through
// the public IContext aggregator like every other contract scenario.
// ---------------------------------------------------------------------------

#include "fixtures/engine_fixture.h"

#include "vigine/api/context/icontext.h"
#include "vigine/api/statemachine/istatemachine.h"
#include "vigine/api/statemachine/stateid.h"
#include "vigine/result.h"

#include <gtest/gtest.h>

#include <thread>

namespace vigine::contract
{
namespace
{

using FsmThreadAffinity = EngineFixture;

// Death-test fixture alias declared up-front so both Debug-only
// EXPECT_DEATH cases (Case 1b and Case 2) can share it. gtest
// reorders DeathTest fixtures to run before non-death ones; the
// "DeathTest" suffix on the fixture name opts into that ordering and
// silences the "Death tests use fork()" warning emitted in some
// sanitiser configurations.
#if !defined(NDEBUG) && defined(GTEST_HAS_DEATH_TEST) && GTEST_HAS_DEATH_TEST
using FsmThreadAffinityDeathTest = EngineFixture;
#endif

// -- Case 1 ------------------------------------------------------------------
//
// One-shot rebind rejection. The contract says
// bindToControllerThread is one-shot: a second call must not
// overwrite the original binding. The implementation reaches that
// by a compare-exchange on a sentinel (default-constructed
// std::thread::id), so the no-overwrite outcome is observable in
// Release without hitting the Debug assert.
//
// We bind the test thread, capture the bound id, then attempt to
// rebind from another thread. Reading back the controller thread
// must still return the original test-thread id.

TEST_F(FsmThreadAffinity, RebindIsRejectedAndDoesNotOverwrite)
{
    auto &sm = context().stateMachine();

    const auto testThreadId = std::this_thread::get_id();
    sm.bindToControllerThread(testThreadId);
    EXPECT_EQ(sm.controllerThread(), testThreadId)
        << "first bind must record the calling thread id";

    // Second bind from another thread. In Release the CAS path silently
    // keeps the original binding, which is the observable contract we
    // pin here. In Debug the same call would abort via assert, so we
    // skip it from this non-death test; the dedicated DeathTest below
    // exercises the abort surface explicitly without aborting the
    // current test process.
    std::thread worker{[&sm] {
#ifdef NDEBUG
        // Release: the CAS in bindToControllerThread fails silently,
        // so calling it from the worker is a no-op.
        sm.bindToControllerThread(std::this_thread::get_id());
#else
        // Debug: do nothing here. The
        // FsmThreadAffinityDeathTest.SecondBindAbortsInDebug case below
        // is the surface that fires the assert in isolation.
        (void)sm;
#endif
    }};
    worker.join();

    EXPECT_EQ(sm.controllerThread(), testThreadId)
        << "rebind must not overwrite the original controller thread id";
}

// -- Case 1b -----------------------------------------------------------------
//
// Debug-only: the second bindToControllerThread call must fire the
// rebind assert. Case 1 above pins the *observable* outcome (no
// overwrite) which is portable across Debug and Release; this case
// pins the *fatal* surface that Debug builds also expose, by spawning
// a child gtest process that performs two binds in a row.
//
// Without this case the documented "second bind asserts" path would
// never actually be exercised by the suite -- a regression that
// quietly turned the assert into a no-op would slip through.

#if !defined(NDEBUG) && defined(GTEST_HAS_DEATH_TEST) && GTEST_HAS_DEATH_TEST

TEST_F(FsmThreadAffinityDeathTest, SecondBindAbortsInDebug)
{
    auto &sm = context().stateMachine();

    // Use threadsafe style for the same reason as Case 2: the
    // multi-threaded parent must not fork() with partially-locked
    // mutexes.
    GTEST_FLAG_SET(death_test_style, "threadsafe");

    EXPECT_DEATH(
        ([&sm] {
            // First bind succeeds in the child process (the death-test
            // child re-execs into a fresh FSM under threadsafe style).
            sm.bindToControllerThread(std::this_thread::get_id());

            // Spawn a worker so the second bind comes from a thread
            // that is genuinely different from the one that took the
            // CAS slot. The assert inside bindToControllerThread fires,
            // the child aborts, the parent observes the death-test
            // pass.
            std::thread worker{[&sm] {
                sm.bindToControllerThread(std::this_thread::get_id());
            }};
            worker.join();
        })(),
        ".*");
}

#endif // !NDEBUG && GTEST_HAS_DEATH_TEST

// -- Case 2 ------------------------------------------------------------------
//
// Wrong-thread sync mutation aborts in Debug.
//
// A fresh fixture binds the test thread as the controller, then a
// worker thread calls transition() through the EXPECT_DEATH
// statement. Debug builds fire the assert inside checkThreadAffinity,
// the child gtest process aborts, and the parent observes the
// death-test pass. Release builds skip the case because the
// affinity gate is compiled out.

#if !defined(NDEBUG) && defined(GTEST_HAS_DEATH_TEST) && GTEST_HAS_DEATH_TEST

// FsmThreadAffinityDeathTest fixture is declared once at the top of
// the anonymous namespace so it is in scope for every Debug-only
// EXPECT_DEATH case below (Case 1b and Case 2).
TEST_F(FsmThreadAffinityDeathTest, WrongThreadSyncMutationAbortsInDebug)
{
    auto &sm = context().stateMachine();

    sm.bindToControllerThread(std::this_thread::get_id());

    // Add an extra state so transition() has a valid target. The
    // worker thread itself is the contract violation; the target id
    // does not matter as long as it is a registered state.
    const auto target = sm.addState();
    ASSERT_TRUE(target.valid());

    // Use the threadsafe death-test style on Windows / multi-threaded
    // hosts so the child process is spawned via re-exec instead of
    // fork(). The default style is sometimes `fast` which on
    // multi-threaded parents can deadlock if a child inherits a
    // partially-locked mutex.
    GTEST_FLAG_SET(death_test_style, "threadsafe");

    EXPECT_DEATH(
        ([&sm, target] {
            std::thread worker{[&sm, target] {
                // This call is from the wrong thread: the assert in
                // AbstractStateMachine::checkThreadAffinity fires,
                // the child process aborts. The Result return value
                // is ignored on the death path.
                (void)sm.transition(target);
            }};
            worker.join();
        })(),
        ".*");
}

#else // Release / no death-test support

TEST_F(FsmThreadAffinity, WrongThreadCaseSkippedInRelease)
{
    SUCCEED()
        << "checkThreadAffinity is compiled out in Release; the assert "
           "case is exercised only in Debug builds.";
}

#endif // NDEBUG / GTEST_HAS_DEATH_TEST

} // namespace
} // namespace vigine::contract

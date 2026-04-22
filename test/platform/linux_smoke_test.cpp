#if defined(__linux__)

#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

#include <gtest/gtest.h>

#include "vigine/eventscheduler/iossignalsource.h"
#include "vigine/eventscheduler/ossignal.h"

// Include the concrete Linux POSIX source directly (it lives in src/).
#include "platform/linux/iossignalsource_posix.h"
#include "platform/linux/xcbwindowbackend.h"

using namespace vigine::eventscheduler;
using namespace vigine::platform::linux_;

// ---------------------------------------------------------------------------
// Scenario 1: XCB window — open, query, close
// ---------------------------------------------------------------------------

TEST(LinuxPlatformSmoke, XcbWindowOpenAndClose)
{
    XcbWindowBackend win("vigine-smoke", 320, 240);

    // On a headless CI runner the X server may not be present.
    // Skip gracefully rather than failing if the connection is unavailable.
    if (!win.isValid())
    {
        GTEST_SKIP() << "No X server available — skipping XCB window test";
    }

    EXPECT_NE(win.connection(), nullptr);
    EXPECT_NE(win.windowId(), static_cast<xcb_window_t>(0));
    EXPECT_EQ(win.width(),  320u);
    EXPECT_EQ(win.height(), 240u);

    win.close();
    // Destructor joins the event thread; no hang expected within the gtest timeout.
}

// ---------------------------------------------------------------------------
// Scenario 2: POSIX self-pipe signal delivery (SIGTERM via self-pipe)
// ---------------------------------------------------------------------------

TEST(LinuxPlatformSmoke, PosixSignalDeliveredViaSelfPipe)
{
    PosixOsSignalSource src;

    std::atomic<int> callCount{0};

    struct TestListener : IOsSignalListener
    {
        std::atomic<int> *counter{nullptr};
        void onOsSignal(OsSignal /*signal*/) override
        {
            counter->fetch_add(1, std::memory_order_relaxed);
        }
    };

    TestListener listener;
    listener.counter = &callCount;

    auto result = src.subscribe(OsSignal::Terminate, &listener);
    ASSERT_EQ(result.code(), vigine::Result::Code::Success);

    // Deliver SIGTERM to ourselves via the self-pipe (simulates the kernel path).
    ::raise(SIGTERM);

    // Give the reader thread time to process.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (callCount.load(std::memory_order_relaxed) == 0 &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_GE(callCount.load(std::memory_order_relaxed), 1);

    src.unsubscribe(OsSignal::Terminate, &listener);
}

// ---------------------------------------------------------------------------
// Scenario 3: SIGUSR1 / SIGUSR2 subscribe succeeds on Linux
// ---------------------------------------------------------------------------

TEST(LinuxPlatformSmoke, UserSignalsSubscribeSucceeds)
{
    PosixOsSignalSource src;

    struct NullListener : IOsSignalListener
    {
        void onOsSignal(OsSignal) override {}
    };

    NullListener listener;

    auto r1 = src.subscribe(OsSignal::User1, &listener);
    EXPECT_EQ(r1.code(), vigine::Result::Code::Success);

    auto r2 = src.subscribe(OsSignal::User2, &listener);
    EXPECT_EQ(r2.code(), vigine::Result::Code::Success);
}

#endif // __linux__

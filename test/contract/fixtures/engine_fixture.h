#pragma once

// ---------------------------------------------------------------------------
// Shared fixture for the full-contract suite.
//
// The contract suite exercises the unified substrate end-to-end:
// threading -> messaging -> facades -> wrappers. The natural aggregator
// for "a live engine in a test-friendly config" is the public IContext
// returned by vigine::context::createContext(): it owns the thread
// manager, the system message bus, the ECS, the state-machine stub, and
// the task-flow stub in the strict construction order encoded by
// AbstractContext (threading -> systemBus -> wrappers -> services).
//
// Each scenario inherits EngineFixture, grabs the aggregator through
// context(), and tears down cleanly in the fixture destructor by
// dropping the std::unique_ptr. The aggregator's destructor walks the
// reverse of the construction chain: services -> wrappers -> systemBus
// -> threading. No explicit shutdown() call is required here; the
// destructor is authoritative.
//
// For scenarios that want to exercise a specific facade over the raw
// bus + thread-manager pair without going through the context
// aggregator (for example scenarios that want a second independent
// user-bus, or that spin a private bus with InlineOnly dispatch), a
// second builder helper makePrivateStack() returns a freshly-built
// thread-manager + bus pair owned by the caller. The helper exists so
// that facades that need their own bus / thread-manager references do
// not have to reach into the aggregator's internals.
//
// Invariants:
//   - No templates leak into the fixture public surface (INV-1 / INV-10).
//   - No graph types mentioned here (INV-11).
//   - Strict encapsulation: every member is private; access through
//     methods only (project_vigine_strict_encapsulation).
// ---------------------------------------------------------------------------

#include "vigine/api/context/contextconfig.h"
#include "vigine/api/context/factory.h"
#include "vigine/api/context/icontext.h"
#include "vigine/messaging/busconfig.h"
#include "vigine/messaging/factory.h"
#include "vigine/messaging/imessagebus.h"
#include "vigine/core/threading/factory.h"
#include "vigine/core/threading/ithreadmanager.h"
#include "vigine/core/threading/threadmanagerconfig.h"

#include <gtest/gtest.h>

#include <memory>
#include <utility>

namespace vigine::contract
{

/**
 * @brief Owning pair of a thread manager + a message bus for tests that
 *        want a private stack independent of the context aggregator.
 *
 * Kept trivially movable so the fixture helper can return it by value.
 */
class PrivateStack
{
  public:
    PrivateStack() = default;

    PrivateStack(std::unique_ptr<vigine::core::threading::IThreadManager> tm,
                 std::unique_ptr<vigine::messaging::IMessageBus>    bus) noexcept
        : _tm(std::move(tm))
        , _bus(std::move(bus))
    {
    }

    ~PrivateStack()
    {
        // Destroy bus before thread manager: the bus holds a reference
        // to the thread manager and must not outlive it.
        _bus.reset();
        _tm.reset();
    }

    PrivateStack(const PrivateStack &)            = delete;
    PrivateStack &operator=(const PrivateStack &) = delete;
    PrivateStack(PrivateStack &&) noexcept        = default;
    PrivateStack &operator=(PrivateStack &&) noexcept = default;

    [[nodiscard]] vigine::core::threading::IThreadManager &threadManager() noexcept
    {
        return *_tm;
    }

    [[nodiscard]] vigine::messaging::IMessageBus &bus() noexcept
    {
        return *_bus;
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return _tm != nullptr && _bus != nullptr;
    }

  private:
    std::unique_ptr<vigine::core::threading::IThreadManager> _tm;
    std::unique_ptr<vigine::messaging::IMessageBus>    _bus;
};

/**
 * @brief Fixture that constructs a full IContext aggregator per test.
 *
 * The aggregator is built from a default-constructed ContextConfig, so
 * every test gets:
 *   - A hardware-concurrency thread pool.
 *   - A system bus named "system" on a dedicated thread.
 *   - Default-constructed ECS / state-machine / task-flow wrappers.
 *
 * Tests that need an InlineOnly bus (so dispatch is synchronous and the
 * test can assert immediately after post) construct a second stack via
 * makePrivateStack(). Tests that need a private Dedicated bus for
 * actor / pipeline / reactive-stream facades do the same.
 */
class EngineFixture : public ::testing::Test
{
  public:
    /**
     * @brief Returns the context aggregator. Valid for the lifetime of
     *        the fixture.
     */
    [[nodiscard]] vigine::IContext &context() noexcept { return *_context; }

    /**
     * @brief Constructs a second, independent thread-manager + bus pair.
     *
     * @p inline When @c true, the bus uses @c ThreadingPolicy::InlineOnly
     *           so post() dispatches synchronously on the caller's
     *           thread -- the deterministic shape the scenarios rely on
     *           for immediate assertion after publish / emit / send.
     */
    [[nodiscard]] PrivateStack makePrivateStack(bool inlineOnly = true)
    {
        vigine::core::threading::ThreadManagerConfig tmCfg{};
        auto tm = vigine::core::threading::createThreadManager(tmCfg);
        if (!tm)
        {
            return {};
        }

        vigine::messaging::BusConfig busCfg{};
        busCfg.name         = "contract-private";
        busCfg.priority     = vigine::messaging::BusPriority::Normal;
        busCfg.threading    = inlineOnly
                                  ? vigine::messaging::ThreadingPolicy::InlineOnly
                                  : vigine::messaging::ThreadingPolicy::Shared;
        busCfg.capacity     = vigine::messaging::QueueCapacity{64, true};
        busCfg.backpressure = vigine::messaging::BackpressurePolicy::Block;

        auto bus = vigine::messaging::createMessageBus(busCfg, *tm);
        if (!bus)
        {
            return {};
        }

        return PrivateStack{std::move(tm), std::move(bus)};
    }

  protected:
    void SetUp() override
    {
        _context = vigine::context::createContext(vigine::context::ContextConfig{});
        ASSERT_NE(_context, nullptr) << "createContext must return a live aggregator";
    }

    void TearDown() override
    {
        // Dropping the unique_ptr runs AbstractContext's destructor,
        // which tears down services -> wrappers -> systemBus -> threading.
        _context.reset();
    }

  private:
    std::unique_ptr<vigine::IContext> _context;
};

} // namespace vigine::contract

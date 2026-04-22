#include "vigine/payload/factory.h"
#include "vigine/payload/ipayloadregistry.h"
#include "vigine/payload/payloadrange.h"
#include "vigine/payload/payloadtypeid.h"
#include "vigine/result.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace vigine;
using namespace vigine::payload;

TEST(DefaultPayloadRegistrySmoke, FactoryReturnsUniquePtr)
{
    std::unique_ptr<IPayloadRegistry> registry = createPayloadRegistry();
    ASSERT_NE(registry, nullptr);
}

TEST(DefaultPayloadRegistrySmoke, EngineRangesPreRegisteredAtBootstrap)
{
    std::unique_ptr<IPayloadRegistry> registry = createPayloadRegistry();

    EXPECT_TRUE(registry->isRegistered(PayloadTypeId{kControlBegin}));
    EXPECT_TRUE(registry->isRegistered(PayloadTypeId{kControlEnd}));
    EXPECT_TRUE(registry->isRegistered(PayloadTypeId{kSystemBegin}));
    EXPECT_TRUE(registry->isRegistered(PayloadTypeId{kSystemEnd}));
    EXPECT_TRUE(registry->isRegistered(PayloadTypeId{kSystemExtBegin}));
    EXPECT_TRUE(registry->isRegistered(PayloadTypeId{kSystemExtEnd}));
    EXPECT_TRUE(registry->isRegistered(PayloadTypeId{kReservedBegin}));
    EXPECT_TRUE(registry->isRegistered(PayloadTypeId{kReservedEnd}));

    // `kUserBegin` (0x10000) is the first identifier in the user half
    // of the split. Until the application registers a range that
    // covers it, it reports as not-registered.
    EXPECT_FALSE(registry->isRegistered(PayloadTypeId{kUserBegin}));

    const auto owner = registry->resolve(PayloadTypeId{kControlBegin});
    ASSERT_TRUE(owner.has_value());
    EXPECT_EQ(*owner, std::string{kEngineOwner});
}

TEST(DefaultPayloadRegistrySmoke, HappyPathUserRegistration)
{
    std::unique_ptr<IPayloadRegistry> registry = createPayloadRegistry();

    const Result r = registry->registerRange(PayloadTypeId{0x20000u},
                                             PayloadTypeId{0x200FFu},
                                             "app.game");
    EXPECT_TRUE(r.isSuccess());
    EXPECT_TRUE(registry->isRegistered(PayloadTypeId{0x20000u}));
    EXPECT_TRUE(registry->isRegistered(PayloadTypeId{0x200FFu}));

    const auto owner = registry->resolve(PayloadTypeId{0x20050u});
    ASSERT_TRUE(owner.has_value());
    EXPECT_EQ(*owner, "app.game");
}

TEST(DefaultPayloadRegistrySmoke, DuplicateRangeRejected)
{
    std::unique_ptr<IPayloadRegistry> registry = createPayloadRegistry();

    const Result first = registry->registerRange(PayloadTypeId{0x30000u},
                                                 PayloadTypeId{0x300FFu},
                                                 "app.a");
    ASSERT_TRUE(first.isSuccess());

    const Result collide = registry->registerRange(PayloadTypeId{0x30080u},
                                                   PayloadTypeId{0x301FFu},
                                                   "app.b");
    EXPECT_TRUE(collide.isError());
    EXPECT_EQ(collide.code(), Result::Code::DuplicatePayloadId);
}

TEST(DefaultPayloadRegistrySmoke, EngineRangesRejectReRegistration)
{
    std::unique_ptr<IPayloadRegistry> registry = createPayloadRegistry();

    const Result r = registry->registerRange(PayloadTypeId{kSystemBegin},
                                             PayloadTypeId{kSystemEnd},
                                             "app.evil");
    EXPECT_TRUE(r.isError());
    EXPECT_EQ(r.code(), Result::Code::DuplicatePayloadId);
}

TEST(DefaultPayloadRegistrySmoke, OutOfRangeRejectedForCrossingBoundary)
{
    std::unique_ptr<IPayloadRegistry> registry = createPayloadRegistry();

    // Spans the engine / user boundary.
    const Result r = registry->registerRange(PayloadTypeId{0xFF00u},
                                             PayloadTypeId{0x20000u},
                                             "app.boundary");
    EXPECT_TRUE(r.isError());
    EXPECT_EQ(r.code(), Result::Code::OutOfRange);
}

TEST(DefaultPayloadRegistrySmoke, OutOfRangeRejectedForInvertedRange)
{
    std::unique_ptr<IPayloadRegistry> registry = createPayloadRegistry();

    const Result r = registry->registerRange(PayloadTypeId{0x30010u},
                                             PayloadTypeId{0x30000u},
                                             "app.inverted");
    EXPECT_TRUE(r.isError());
    EXPECT_EQ(r.code(), Result::Code::OutOfRange);
}

TEST(DefaultPayloadRegistrySmoke, UnregisterFreesRangeForReuse)
{
    std::unique_ptr<IPayloadRegistry> registry = createPayloadRegistry();

    ASSERT_TRUE(registry
                    ->registerRange(PayloadTypeId{0x40000u},
                                    PayloadTypeId{0x400FFu},
                                    "app.transient")
                    .isSuccess());
    ASSERT_TRUE(registry->isRegistered(PayloadTypeId{0x40050u}));

    ASSERT_TRUE(registry->unregister("app.transient").isSuccess());
    EXPECT_FALSE(registry->isRegistered(PayloadTypeId{0x40050u}));

    // Reuse the now-free range under a different owner.
    const Result reuse = registry->registerRange(PayloadTypeId{0x40000u},
                                                 PayloadTypeId{0x400FFu},
                                                 "app.other");
    EXPECT_TRUE(reuse.isSuccess());
}

TEST(DefaultPayloadRegistrySmoke, ConcurrentRegisterAndResolveAreTsanClean)
{
    std::unique_ptr<IPayloadRegistry> registry = createPayloadRegistry();

    constexpr int kWriters    = 4;
    constexpr int kReaders    = 4;
    constexpr int kPerWriter  = 32;
    std::atomic<bool> startFlag{false};
    std::atomic<int>  registered{0};

    std::vector<std::thread> workers;
    workers.reserve(kWriters + kReaders);

    for (int w = 0; w < kWriters; ++w)
    {
        workers.emplace_back([&, w]() {
            while (!startFlag.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
            const std::uint32_t base = 0x50000u + static_cast<std::uint32_t>(w) * 0x10000u;
            for (int i = 0; i < kPerWriter; ++i)
            {
                const std::uint32_t lo = base + static_cast<std::uint32_t>(i) * 0x10u;
                const std::uint32_t hi = lo + 0x0Fu;
                if (registry
                        ->registerRange(PayloadTypeId{lo},
                                        PayloadTypeId{hi},
                                        "app.concurrent")
                        .isSuccess())
                {
                    registered.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (int r = 0; r < kReaders; ++r)
    {
        workers.emplace_back([&]() {
            while (!startFlag.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
            for (std::uint32_t probe = 0x50000u; probe < 0x90000u; probe += 0x80u)
            {
                static_cast<void>(registry->isRegistered(PayloadTypeId{probe}));
                static_cast<void>(registry->resolve(PayloadTypeId{probe}));
            }
        });
    }

    startFlag.store(true, std::memory_order_release);
    for (std::thread &t : workers)
    {
        t.join();
    }

    EXPECT_EQ(registered.load(std::memory_order_relaxed), kWriters * kPerWriter);
}

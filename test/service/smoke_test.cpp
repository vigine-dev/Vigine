// ---------------------------------------------------------------------------
// Service smoke suite.
//
// The full-contract scenario covers the round trip through the context
// aggregator. This smoke target exercises the wrapper directly via
// createService() so regressions in the IService contract surface
// fail here without the full engine fixture noise:
//
//   * createService returns a live, default-constructed service.
//   * dependencies() reports an empty list on a fresh service.
//   * isInitialised() flips false -> true -> false across onInit +
//     onShutdown.
//   * Calling onInit a second time (idempotency or forward-only
//     semantics) does not flip the flag back to false.
// ---------------------------------------------------------------------------

#include "vigine/context/icontext.h"
#include "vigine/result.h"
#include "vigine/service/factory.h"
#include "vigine/service/iservice.h"

#include <gtest/gtest.h>

#include <memory>

namespace
{

// Minimal IContext test double. The default IService base only reads
// `context` through its onInit / onShutdown hooks; we never invoke an
// aggregator method on it, so every accessor throws to catch accidental
// use. If a future service base starts driving an aggregator method
// during lifecycle, the smoke test will fail loud here.
class ThrowingContext final : public vigine::IContext
{
  public:
    [[nodiscard]] vigine::messaging::IMessageBus &systemBus() override
    {
        throw std::runtime_error{"smoke ThrowingContext: systemBus called"};
    }

    [[nodiscard]] std::shared_ptr<vigine::messaging::IMessageBus>
        createMessageBus(const vigine::messaging::BusConfig &) override
    {
        throw std::runtime_error{"smoke ThrowingContext: createMessageBus called"};
    }

    [[nodiscard]] std::shared_ptr<vigine::messaging::IMessageBus>
        messageBus(vigine::messaging::BusId) const override
    {
        throw std::runtime_error{"smoke ThrowingContext: messageBus called"};
    }

    [[nodiscard]] vigine::ecs::IECS &ecs() override
    {
        throw std::runtime_error{"smoke ThrowingContext: ecs called"};
    }

    [[nodiscard]] vigine::statemachine::IStateMachine &stateMachine() override
    {
        throw std::runtime_error{"smoke ThrowingContext: stateMachine called"};
    }

    [[nodiscard]] vigine::taskflow::ITaskFlow &taskFlow() override
    {
        throw std::runtime_error{"smoke ThrowingContext: taskFlow called"};
    }

    [[nodiscard]] vigine::threading::IThreadManager &threadManager() override
    {
        throw std::runtime_error{"smoke ThrowingContext: threadManager called"};
    }

    [[nodiscard]] std::shared_ptr<vigine::service::IService>
        service(vigine::service::ServiceId) const override
    {
        throw std::runtime_error{"smoke ThrowingContext: service called"};
    }

    [[nodiscard]] vigine::Result
        registerService(std::shared_ptr<vigine::service::IService>) override
    {
        throw std::runtime_error{"smoke ThrowingContext: registerService called"};
    }

    void freeze() noexcept override {}

    [[nodiscard]] bool isFrozen() const noexcept override { return false; }
};

} // namespace

TEST(ServiceSmoke, FactoryHandsBackLiveService)
{
    auto svc = vigine::service::createService();
    ASSERT_NE(svc, nullptr);
    EXPECT_FALSE(svc->isInitialised());
    EXPECT_FALSE(svc->id().valid());  // no stamping before registration
}

TEST(ServiceSmoke, DependenciesListIsEmptyOnFreshService)
{
    auto svc = vigine::service::createService();
    ASSERT_NE(svc, nullptr);

    const auto deps = svc->dependencies();
    EXPECT_TRUE(deps.empty());
}

TEST(ServiceSmoke, OnInitFlipsInitialisedOnShutdownFlipsItBack)
{
    auto svc = vigine::service::createService();
    ASSERT_NE(svc, nullptr);

    ThrowingContext ctx;

    EXPECT_FALSE(svc->isInitialised());

    const vigine::Result initR = svc->onInit(ctx);
    EXPECT_TRUE(initR.isSuccess());
    EXPECT_TRUE(svc->isInitialised());

    const vigine::Result shutdownR = svc->onShutdown(ctx);
    EXPECT_TRUE(shutdownR.isSuccess());
    EXPECT_FALSE(svc->isInitialised());
}

TEST(ServiceSmoke, RepeatedOnInitIsSafeAndKeepsInitialisedTrue)
{
    auto svc = vigine::service::createService();
    ASSERT_NE(svc, nullptr);

    ThrowingContext ctx;

    ASSERT_TRUE(svc->onInit(ctx).isSuccess());
    EXPECT_TRUE(svc->isInitialised());

    // Calling onInit again must not flip the flag off. Concrete services
    // may choose between idempotency or reject-with-error, but the
    // lifecycle flag must never regress to false while still active.
    const vigine::Result secondInit = svc->onInit(ctx);
    (void)secondInit;
    EXPECT_TRUE(svc->isInitialised());
}

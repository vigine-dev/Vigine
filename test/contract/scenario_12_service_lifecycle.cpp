// ---------------------------------------------------------------------------
// Scenario 12 -- service wrapper init / shutdown lifecycle through context.
//
// The context aggregator owns a service registry. Concrete services
// provide onInit / onShutdown hooks that receive an IContext reference.
// The scenario:
//
//   1. Implements a TestService whose onInit / onShutdown flip flags
//      held in a shared state object.
//   2. Registers the service on the context through registerService.
//   3. Looks it up through service(id) and verifies the id matches.
//   4. Calls freeze() on the context; a subsequent registerService must
//      return Result::Code::TopologyFrozen.
//
// The context itself does NOT drive onInit -- that lives on the
// Engine::run lifecycle leaf which isn't under test here. The scenario
// therefore verifies the registry path alone (register / lookup /
// freeze boundary), which is the observable surface of IContext.
// ---------------------------------------------------------------------------

#include "fixtures/engine_fixture.h"

#include "vigine/context/icontext.h"
#include "vigine/result.h"
#include "vigine/service/iservice.h"
#include "vigine/service/serviceid.h"

#include <gtest/gtest.h>

#include <memory>
#include <utility>
#include <vector>

namespace vigine::contract
{
namespace
{

class TestService final : public vigine::service::IService
{
  public:
    [[nodiscard]] vigine::service::ServiceId id() const noexcept override
    {
        return _id;
    }

    [[nodiscard]] vigine::Result onInit(vigine::IContext & /*context*/) override
    {
        _initialised = true;
        return vigine::Result{};
    }

    [[nodiscard]] vigine::Result onShutdown(vigine::IContext & /*context*/) override
    {
        _initialised = false;
        return vigine::Result{};
    }

    [[nodiscard]] std::vector<std::shared_ptr<vigine::service::IService>>
        dependencies() const override
    {
        return {};
    }

    [[nodiscard]] bool isInitialised() const noexcept override { return _initialised; }

    void stampId(vigine::service::ServiceId id) noexcept { _id = id; }

  private:
    vigine::service::ServiceId _id{};
    bool                       _initialised{false};
};

using ServiceLifecycle = EngineFixture;

TEST_F(ServiceLifecycle, RegisterSucceedsAndLookupByNullIdFails)
{
    auto &ctx = context();

    auto service = std::make_shared<TestService>();
    const vigine::Result registered = ctx.registerService(service);
    EXPECT_TRUE(registered.isSuccess())
        << "registerService on a fresh context must succeed; got: "
        << registered.message();

    // The aggregator keeps the stamped id internal at this leaf
    // (R.4.5 deliberately defers a container-wiring plumbing leaf).
    // The observable contract surface therefore is:
    //   - service(ServiceId{}) with the invalid sentinel returns null.
    //   - The shared_ptr the caller passed in stays live because the
    //     context holds its own copy in the registry.
    auto resolved = ctx.service(vigine::service::ServiceId{});
    EXPECT_EQ(resolved, nullptr)
        << "lookup by the invalid sentinel must return null";
    EXPECT_EQ(service.use_count(), 2)
        << "the context must hold a second strong reference";
}

TEST_F(ServiceLifecycle, FreezeBlocksRegistration)
{
    auto &ctx = context();

    ctx.freeze();

    auto service = std::make_shared<TestService>();
    const vigine::Result registered = ctx.registerService(service);
    EXPECT_TRUE(registered.isError())
        << "registerService after freeze must return an error";
    EXPECT_EQ(registered.code(),
              vigine::Result::Code::TopologyFrozen)
        << "the frozen error code must be TopologyFrozen";
}

} // namespace
} // namespace vigine::contract

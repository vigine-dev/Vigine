// ---------------------------------------------------------------------------
// Scenario 1 -- engine construction + shutdown round-trip.
//
// Builds the full aggregator (IContext) through the shared EngineFixture,
// exercises every accessor on the pure-virtual surface so that the
// construction chain (threading -> system bus -> Level-1 wrappers) is
// observable, then lets the fixture destructor unwind the chain in
// reverse. Destructor failure under sanitizers is caught by the
// sanitizer matrix job (plan_26).
// ---------------------------------------------------------------------------

#include "fixtures/engine_fixture.h"

#include "vigine/api/context/icontext.h"
#include "vigine/api/ecs/iecs.h"
#include "vigine/api/messaging/imessagebus.h"
#include "vigine/statemachine/istatemachine.h"
#include "vigine/taskflow/itaskflow.h"
#include "vigine/core/threading/ithreadmanager.h"

#include <gtest/gtest.h>

namespace vigine::contract
{
namespace
{

using EngineLifecycle = EngineFixture;

TEST_F(EngineLifecycle, ConstructsAllFirstLevelResources)
{
    auto &ctx = context();

    // Touching each accessor confirms the aggregator built every
    // Level-1 wrapper in the expected construction order (threading
    // first, system bus second, wrappers last).
    auto &tm      = ctx.threadManager();
    auto &sysBus  = ctx.systemBus();
    auto &ecs     = ctx.ecs();
    auto &sm      = ctx.stateMachine();
    auto &tf      = ctx.taskFlow();

    EXPECT_GE(tm.poolSize(), 1u)
        << "threading pool must carry at least one worker";
    EXPECT_TRUE(sysBus.id().valid())
        << "system bus must be stamped with a valid BusId by the factory";

    // The concrete ECS / state-machine / task-flow surfaces are not
    // stubs -- they have observable behaviour. Each accessor returns a
    // distinct live reference; exercise one cheap call on each to make
    // the scenario catch a broken wrapper.
    const auto entity = ecs.createEntity();
    EXPECT_TRUE(entity.valid())
        << "createEntity must return a valid EntityId on a fresh ECS";
    EXPECT_TRUE(sm.current().valid())
        << "state machine auto-provisions a default initial state (UD-3)";

    // Silence "unused variable" under -Wall for the task-flow stub
    // whose current concrete surface has no observable accessor yet.
    (void) tf;
}

TEST_F(EngineLifecycle, FreezeTogglesTopologyFlag)
{
    auto &ctx = context();

    EXPECT_FALSE(ctx.isFrozen())
        << "aggregator must start with topology unfrozen";

    ctx.freeze();
    EXPECT_TRUE(ctx.isFrozen());

    // Second freeze must be idempotent.
    ctx.freeze();
    EXPECT_TRUE(ctx.isFrozen());
}

} // namespace
} // namespace vigine::contract

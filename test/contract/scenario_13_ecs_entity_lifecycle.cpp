// ---------------------------------------------------------------------------
// Scenario 13 -- ECS entity lifecycle via wrapper.
//
// Exercises the full entity round-trip through the IECS wrapper owned
// by the context aggregator:
//
//   1. Create an entity; assert the returned EntityId is valid.
//   2. Attach a TestComponent; assert the ComponentHandle is valid and
//      findComponent resolves to the same component.
//   3. Iterate via entitiesWith(typeId); assert the list contains the
//      new entity.
//   4. Destroy the entity; assert hasEntity returns false and the
//      component is no longer reachable.
// ---------------------------------------------------------------------------

#include "fixtures/engine_fixture.h"

#include "vigine/context/icontext.h"
#include "vigine/ecs/ecstypes.h"
#include "vigine/ecs/iecs.h"
#include "vigine/result.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace vigine::contract
{
namespace
{

constexpr vigine::ecs::ComponentTypeId kTestComponentType = 0x91001u;

class TestComponent final : public vigine::ecs::IComponent
{
  public:
    explicit TestComponent(int value) noexcept : _value(value) {}

    [[nodiscard]] vigine::ecs::ComponentTypeId
        componentTypeId() const noexcept override
    {
        return kTestComponentType;
    }

    [[nodiscard]] int value() const noexcept { return _value; }

  private:
    int _value;
};

using EcsLifecycle = EngineFixture;

TEST_F(EcsLifecycle, CreateAttachIterateDestroy)
{
    auto &ecs = context().ecs();

    const auto entity = ecs.createEntity();
    ASSERT_TRUE(entity.valid());
    EXPECT_TRUE(ecs.hasEntity(entity));

    const auto handle = ecs.attachComponent(
        entity, std::make_unique<TestComponent>(/*value=*/7));
    EXPECT_TRUE(handle.valid())
        << "attachComponent on a live entity must return a valid handle";

    const auto *found = ecs.findComponent(entity, kTestComponentType);
    ASSERT_NE(found, nullptr);
    const auto *downcast = dynamic_cast<const TestComponent *>(found);
    ASSERT_NE(downcast, nullptr);
    EXPECT_EQ(downcast->value(), 7);

    const auto carriers = ecs.entitiesWith(kTestComponentType);
    bool seen = false;
    for (const auto &id : carriers)
    {
        if (id == entity)
        {
            seen = true;
            break;
        }
    }
    EXPECT_TRUE(seen) << "entitiesWith must list the newly-attached entity";

    const vigine::Result removed = ecs.removeEntity(entity);
    EXPECT_TRUE(removed.isSuccess())
        << "removeEntity on a live id must succeed; got: " << removed.message();
    EXPECT_FALSE(ecs.hasEntity(entity))
        << "hasEntity must report false after removeEntity";

    const auto *stale = ecs.findComponent(entity, kTestComponentType);
    EXPECT_EQ(stale, nullptr)
        << "component lookup on a removed entity must return nullptr";
}

} // namespace
} // namespace vigine::contract

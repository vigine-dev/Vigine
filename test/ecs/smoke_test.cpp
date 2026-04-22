// ---------------------------------------------------------------------------
// ECS smoke suite.
//
// The full-contract scenario covers the happy-path round trip through
// the context-owned ECS. This target exercises the wrapper directly
// via createECS() and pushes churn at the entity + component layer
// (1k entities, two components each, removal + re-attach) so a
// regression in the underlying storage — the tombstone accumulation
// called out in the follow-up log, a missed cascade on removeEntity,
// a failure in entitiesWith iteration under churn — fails loudly in
// isolation.
// ---------------------------------------------------------------------------

#include "vigine/ecs/ecstypes.h"
#include "vigine/ecs/factory.h"
#include "vigine/ecs/iecs.h"
#include "vigine/result.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{

constexpr vigine::ecs::ComponentTypeId kTagA = 0x91101u;
constexpr vigine::ecs::ComponentTypeId kTagB = 0x91102u;

class TagA final : public vigine::ecs::IComponent
{
  public:
    [[nodiscard]] vigine::ecs::ComponentTypeId
        componentTypeId() const noexcept override
    {
        return kTagA;
    }
};

class TagB final : public vigine::ecs::IComponent
{
  public:
    [[nodiscard]] vigine::ecs::ComponentTypeId
        componentTypeId() const noexcept override
    {
        return kTagB;
    }
};

struct NodeIdHashPolicy
{
    std::size_t operator()(vigine::ecs::EntityId id) const noexcept
    {
        const std::uint64_t key =
            (static_cast<std::uint64_t>(id.index) << 32)
          | static_cast<std::uint64_t>(id.generation);
        if constexpr (sizeof(std::size_t) >= sizeof(std::uint64_t))
        {
            return static_cast<std::size_t>(key);
        }
        else
        {
            return static_cast<std::size_t>(key ^ (key >> 32));
        }
    }
};

} // namespace

TEST(EcsSmoke, CreateThousandEntitiesAttachPairIterate)
{
    auto ecs = vigine::ecs::createECS();
    ASSERT_NE(ecs, nullptr);

    constexpr std::size_t kCount = 1000;
    std::vector<vigine::ecs::EntityId> ids;
    ids.reserve(kCount);

    // Phase 1: create + attach two components each.
    for (std::size_t i = 0; i < kCount; ++i)
    {
        const auto id = ecs->createEntity();
        ASSERT_TRUE(id.valid());
        ASSERT_TRUE(ecs->hasEntity(id));

        const auto handleA = ecs->attachComponent(id, std::make_unique<TagA>());
        ASSERT_TRUE(handleA.valid());
        const auto handleB = ecs->attachComponent(id, std::make_unique<TagB>());
        ASSERT_TRUE(handleB.valid());

        ids.push_back(id);
    }

    // Phase 2: entitiesWith on each tag must report every entity we
    // just created — exactly once per entity.
    const auto carriersA = ecs->entitiesWith(kTagA);
    const auto carriersB = ecs->entitiesWith(kTagB);

    EXPECT_EQ(carriersA.size(), kCount);
    EXPECT_EQ(carriersB.size(), kCount);

    std::unordered_set<vigine::ecs::EntityId, NodeIdHashPolicy>
        uniqueA(carriersA.begin(), carriersA.end());
    std::unordered_set<vigine::ecs::EntityId, NodeIdHashPolicy>
        uniqueB(carriersB.begin(), carriersB.end());
    EXPECT_EQ(uniqueA.size(), kCount);
    EXPECT_EQ(uniqueB.size(), kCount);
}

TEST(EcsSmoke, RemoveEntityReleasesAttachedComponents)
{
    auto ecs = vigine::ecs::createECS();
    ASSERT_NE(ecs, nullptr);

    const auto e = ecs->createEntity();
    ASSERT_TRUE(e.valid());

    const auto attach = ecs->attachComponent(e, std::make_unique<TagA>());
    ASSERT_TRUE(attach.valid());

    ASSERT_NE(ecs->findComponent(e, kTagA), nullptr);

    const vigine::Result removed = ecs->removeEntity(e);
    EXPECT_TRUE(removed.isSuccess());
    EXPECT_FALSE(ecs->hasEntity(e));

    EXPECT_EQ(ecs->findComponent(e, kTagA), nullptr);

    const auto carriers = ecs->entitiesWith(kTagA);
    EXPECT_TRUE(std::find(carriers.begin(), carriers.end(), e) == carriers.end());
}

TEST(EcsSmoke, DetachReattachChurn)
{
    auto ecs = vigine::ecs::createECS();
    ASSERT_NE(ecs, nullptr);

    constexpr std::size_t kRounds = 100;
    const auto e = ecs->createEntity();
    ASSERT_TRUE(e.valid());

    for (std::size_t i = 0; i < kRounds; ++i)
    {
        const auto attach = ecs->attachComponent(e, std::make_unique<TagA>());
        ASSERT_TRUE(attach.valid());
        ASSERT_NE(ecs->findComponent(e, kTagA), nullptr);

        const vigine::Result detach = ecs->detachComponent(e, kTagA);
        EXPECT_TRUE(detach.isSuccess());
        EXPECT_EQ(ecs->findComponent(e, kTagA), nullptr);
    }

    // After the churn, the entity still exists; a fresh attach works.
    const auto final_ = ecs->attachComponent(e, std::make_unique<TagA>());
    EXPECT_TRUE(final_.valid());
}

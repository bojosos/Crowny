#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Renderer/RenderSnapshot.h"

#include <type_traits>

using namespace Crowny;

TEST_CASE("2D render order is stable across renderable types", "[Renderer][2D]")
{
    Vector<Renderable2DOrder> items = {
        { Renderable2DType::Text, 0, 1, 0, 30 }, { Renderable2DType::Sprite, 0, -1, 20, 40 }, { Renderable2DType::Sprite, 1, 1, -2, 20 },
        { Renderable2DType::Text, 1, 1, 0, 10 }, { Renderable2DType::Text, 2, 1, 0, 50 },     { Renderable2DType::Sprite, 2, 1, 0, 50 },
    };

    std::sort(items.begin(), items.end(), Renderable2DOrderLess);

    CHECK(items[0].SortingLayer == -1);
    CHECK(items[1].OrderInLayer == -2);
    CHECK(items[2].Type == Renderable2DType::Text);
    CHECK(items[2].StableOrder == 10);
    CHECK(items[3].StableOrder == 30);
    CHECK(items[4].Type == Renderable2DType::Sprite);
    CHECK(items[5].Type == Renderable2DType::Text);
}

TEST_CASE("2D render ordering allocates nothing after warm-up", "[Renderer][2D][Memory][Frame]")
{
    Vector<Renderable2DOrder> items;
    items.reserve(10000);
    for (uint32_t index = 0; index < 10000; index++)
    {
        items.push_back({ index % 2u == 0u ? Renderable2DType::Sprite : Renderable2DType::Text, index, static_cast<int32_t>(index % 7u),
                          static_cast<int32_t>(index % 13u), index / 2u });
    }
    std::sort(items.begin(), items.end(), Renderable2DOrderLess);

    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    for (uint32_t frame = 0; frame < 120; frame++)
    {
        std::reverse(items.begin(), items.end());
        std::sort(items.begin(), items.end(), Renderable2DOrderLess);
    }
    const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();
    const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, after);

    CHECK(std::is_sorted(items.begin(), items.end(), Renderable2DOrderLess));
    CHECK(delta.AllocationCount == 0);
    CHECK(delta.RequestedBytes == 0);
}

TEST_CASE("Legacy snapshot materials stay flat and allocation-free after warm-up", "[Renderer][Memory][Frame]")
{
    constexpr std::array<uint32_t, 3> entityCounts{ 1u, 1000u, 10000u };
    constexpr uint32_t materialsPerEntity = 4u;
    constexpr uint32_t frameCount = 120u;
    const Vector<AssetHandle<Material>> materials(materialsPerEntity);

    using MaterialSetter = bool (RenderSnapshot::*)(RenderableObject&, const Vector<AssetHandle<Material>>&);
    static_assert(!std::is_invocable_v<MaterialSetter, RenderSnapshot&, RenderableObject&,
                                       std::span<const AssetHandle<Material>>>);

    constexpr size_t maxMaterialCount = std::numeric_limits<uint32_t>::max();
    static_assert(RenderSnapshot::CanAppendMaterials(0u, maxMaterialCount));
    static_assert(RenderSnapshot::CanAppendMaterials(maxMaterialCount, 0u));
    static_assert(!RenderSnapshot::CanAppendMaterials(maxMaterialCount, 1u));
    static_assert(!RenderSnapshot::CanAppendMaterials(maxMaterialCount + 1u, 0u));

    for (const uint32_t entityCount : entityCounts)
    {
        RenderSnapshot snapshot;
        snapshot.MeshObjects.Reserve(entityCount);
        snapshot.LegacyMaterials.Reserve(static_cast<size_t>(entityCount) * materialsPerEntity);

        const auto rebuild = [&]() {
            snapshot.Clear();
            bool storedAllMaterials = true;
            for (uint32_t entity = 0; entity < entityCount; entity++)
            {
                RenderableObject& object = snapshot.MeshObjects.Acquire();
                storedAllMaterials = snapshot.SetMaterials(object, materials) && storedAllMaterials;
            }
            return storedAllMaterials;
        };

        REQUIRE(rebuild());
        REQUIRE(snapshot.MeshObjects.Size() == entityCount);
        REQUIRE(snapshot.LegacyMaterials.Size() == static_cast<size_t>(entityCount) * materialsPerEntity);
        CHECK(snapshot.MeshObjects[0].MaterialOffset == 0u);
        CHECK(snapshot.MeshObjects[0].MaterialCount == materialsPerEntity);
        const RenderableObject& middleObject = snapshot.MeshObjects[entityCount / 2u];
        const std::span<const AssetHandle<Material>> middleMaterials = snapshot.GetMaterials(middleObject);
        CHECK(middleMaterials.size() == materialsPerEntity);
        CHECK(middleMaterials.data() == snapshot.LegacyMaterials.begin() + middleObject.MaterialOffset);
        CHECK(snapshot.MeshObjects[entityCount - 1u].MaterialOffset == (entityCount - 1u) * materialsPerEntity);

        const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
        bool storedAllMaterials = true;
        for (uint32_t frame = 0; frame < frameCount; frame++)
            storedAllMaterials = rebuild() && storedAllMaterials;
        const Memory::ThreadAllocationSnapshot delta =
          Memory::GetThreadAllocationDelta(before, Memory::GetThreadAllocationSnapshot());

        INFO("Entity count: " << entityCount);
        CHECK(storedAllMaterials);
        CHECK(delta.AllocationCount == 0u);
        CHECK(delta.RequestedBytes == 0u);

        snapshot.Clear();
        const uint32_t reducedCount = std::max(entityCount / 2u, 1u);
        for (uint32_t entity = 0; entity < reducedCount; entity++)
        {
            RenderableObject& object = snapshot.MeshObjects.Acquire();
            REQUIRE(snapshot.SetMaterials(object, materials));
        }
        CHECK(snapshot.MeshObjects.Size() == reducedCount);
        CHECK(snapshot.LegacyMaterials.Size() == static_cast<size_t>(reducedCount) * materialsPerEntity);
    }
}

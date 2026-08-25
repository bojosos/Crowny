#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Renderer/RenderSnapshot.h"

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

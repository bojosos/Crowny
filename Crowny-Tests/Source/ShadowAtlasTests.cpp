#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/ShadowAtlas.h"

using namespace Crowny;

TEST_CASE("Shadow atlas allocations are stable and retire on the frame timeline", "[Renderer][Lights][Shadows]")
{
    ShadowAtlasAllocator atlas(512, 128);
    const RenderLightHandle first = RenderLightHandle::FromParts(1, 1);
    const ShadowAtlasAllocation allocation = atlas.Acquire(first, 200);
    REQUIRE(allocation.IsValid());
    CHECK(allocation.Size == 256);
    CHECK(atlas.Acquire(first, 200).X == allocation.X);
    REQUIRE(atlas.Release(first, 5));

    const RenderLightHandle second = RenderLightHandle::FromParts(2, 1);
    const ShadowAtlasAllocation beforeRetirement = atlas.Acquire(second, 512);
    CHECK_FALSE(beforeRetirement.IsValid());
    atlas.Collect(4);
    CHECK_FALSE(atlas.Acquire(second, 512).IsValid());
    atlas.Collect(5);
    CHECK(atlas.Acquire(second, 512).IsValid());
}

TEST_CASE("Shadow update scheduling honors update and pixel budgets", "[Renderer][Lights][Shadows]")
{
    const std::array requests{
        ShadowUpdateRequest{ RenderLightHandle::FromParts(1, 1), LightType::Spot, 512, 2.0f, true },
        ShadowUpdateRequest{ RenderLightHandle::FromParts(2, 1), LightType::Point, 512, 1.0f, true },
        ShadowUpdateRequest{ RenderLightHandle::FromParts(3, 1), LightType::Spot, 512, 1.0f, false },
    };
    ShadowUpdateBudget budget;
    budget.MaximumLocalUpdates = 2;
    budget.MaximumPixels = 512ull * 512ull;
    Vector<RenderLightHandle> scheduled;
    uint64_t pixels = 0;
    ShadowUpdateScheduler::Schedule(requests.data(), static_cast<uint32_t>(requests.size()), budget, scheduled, pixels);
    REQUIRE(scheduled.size() == 1);
    CHECK(scheduled[0] == requests[0].Light);
    CHECK(pixels == 512ull * 512ull);
}

#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Renderer/ShadowAtlas.h"

#include <array>
#include <limits>

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
    ShadowUpdateScheduler scheduler;
    Vector<RenderLightHandle> scheduled;
    uint64_t pixels = 0;
    scheduler.Schedule(requests.data(), static_cast<uint32_t>(requests.size()), budget, scheduled, pixels);
    REQUIRE(scheduled.size() == 1);
    CHECK(scheduled[0] == requests[0].Light);
    CHECK(pixels == 512ull * 512ull);
}

TEST_CASE("Cached local shadows invalidate when either the light or caster revision changes", "[Renderer][Lights][Shadows]")
{
    LightShadowSettings settings;
    settings.CacheStaticCasters = true;
    CHECK_FALSE(RequiresShadowCacheRedraw(settings, false, 7, 7));
    CHECK(RequiresShadowCacheRedraw(settings, true, 7, 7));
    CHECK(RequiresShadowCacheRedraw(settings, false, 8, 7));

    settings.CacheStaticCasters = false;
    CHECK(RequiresShadowCacheRedraw(settings, false, 7, 7));
}

TEST_CASE("Shadow update scheduling preserves equal-score input order and filters invalid work", "[Renderer][Lights][Shadows]")
{
    const std::array requests{
        ShadowUpdateRequest{ RenderLightHandle::FromParts(1, 1), LightType::Spot, 256, 1.0f, true },
        ShadowUpdateRequest{ RenderLightHandle::FromParts(2, 1), LightType::Spot, 256, 1.0f, true },
        ShadowUpdateRequest{ RenderLightHandle::FromParts(3, 1), LightType::Spot, 256, 4.0f, false },
        ShadowUpdateRequest{ RenderLightHandle::FromParts(4, 1), LightType::Directional, 256, 4.0f, true },
        ShadowUpdateRequest{ {}, LightType::Spot, 256, 4.0f, true },
    };
    ShadowUpdateBudget budget;
    budget.MaximumLocalUpdates = static_cast<uint32_t>(requests.size());
    budget.MaximumPixels = std::numeric_limits<uint64_t>::max();
    ShadowUpdateScheduler scheduler;
    Vector<RenderLightHandle> scheduled;
    uint64_t pixels = 0;

    scheduler.Schedule(requests.data(), static_cast<uint32_t>(requests.size()), budget, scheduled, pixels);

    REQUIRE(scheduled.size() == 2u);
    CHECK(scheduled[0] == requests[0].Light);
    CHECK(scheduled[1] == requests[1].Light);
    CHECK(pixels == 2ull * 256ull * 256ull);
}

TEST_CASE("Shadow update scheduling reuses scratch storage after warm-up", "[Renderer][Lights][Shadows][Memory][Frame]")
{
    constexpr std::array<uint32_t, 3> requestCounts{ 1u, 1000u, 10000u };
    for (const uint32_t requestCount : requestCounts)
    {
        Vector<ShadowUpdateRequest> requests;
        requests.reserve(requestCount);
        for (uint32_t index = 0; index < requestCount; index++)
            requests.push_back({ RenderLightHandle::FromParts(index + 1u, 1u), LightType::Spot, 128u, 1.0f, true });

        ShadowUpdateBudget budget;
        budget.MaximumLocalUpdates = requestCount;
        budget.MaximumPixels = std::numeric_limits<uint64_t>::max();
        ShadowUpdateScheduler scheduler;
        Vector<RenderLightHandle> scheduled;
        uint64_t pixels = 0;
        scheduler.Schedule(requests.data(), requestCount, budget, scheduled, pixels);

        const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
        uint64_t checksum = 0;
        for (uint32_t frame = 0; frame < 120u; frame++)
        {
            scheduler.Schedule(requests.data(), requestCount, budget, scheduled, pixels);
            checksum += scheduled.size() + pixels + scheduled.front().GetValue();
        }
        const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();
        const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, after);

        CHECK(checksum != 0u);
        CHECK(scheduled.size() == requestCount);
        CHECK(pixels == static_cast<uint64_t>(requestCount) * 128ull * 128ull);
        CHECK(delta.AllocationCount == 0u);
        CHECK(delta.RequestedBytes == 0u);

        requests.push_back({ RenderLightHandle::FromParts(requestCount + 1u, 1u), LightType::Spot, 128u, 1.0f, true });
        budget.MaximumLocalUpdates++;
        scheduler.Schedule(requests.data(), static_cast<uint32_t>(requests.size()), budget, scheduled, pixels);
        const Memory::ThreadAllocationSnapshot afterGrowth = Memory::GetThreadAllocationSnapshot();
        scheduler.Schedule(requests.data(), static_cast<uint32_t>(requests.size()), budget, scheduled, pixels);
        const Memory::ThreadAllocationSnapshot afterStableGrowth = Memory::GetThreadAllocationSnapshot();
        const Memory::ThreadAllocationSnapshot stableGrowthDelta = Memory::GetThreadAllocationDelta(afterGrowth, afterStableGrowth);
        CHECK(scheduled.size() == requests.size());
        CHECK(stableGrowthDelta.AllocationCount == 0u);
        CHECK(stableGrowthDelta.RequestedBytes == 0u);
    }
}

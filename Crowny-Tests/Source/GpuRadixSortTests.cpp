#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/GpuRadixSort.h"

using namespace Crowny;

TEST_CASE("GPU radix sort plans bounded scratch storage", "[Renderer][RadixSort]")
{
    const GpuRadixSortPlan plan = GpuRadixSort::BuildPlan(100000);
    CHECK(plan.ElementCount == 100000);
    CHECK(plan.GroupSize == 256);
    CHECK(plan.GroupCount == 391);
    CHECK(plan.Radix == 256);
    CHECK(plan.PassCount == 8);
    CHECK(plan.HistogramBytes == 391ull * 256ull * sizeof(uint32_t));
    CHECK(plan.GroupOffsetBytes == plan.HistogramBytes);
    CHECK(plan.ScratchKeyBytes == 100000ull * 16ull);
    CHECK(plan.ScratchValueBytes == 100000ull * sizeof(uint32_t));
    CHECK(plan.GetScratchBytes() < 8ull * 1024ull * 1024ull);
}

TEST_CASE("GPU radix digits traverse a 64-bit key from least significant byte", "[Renderer][RadixSort]")
{
    constexpr uint32_t low = 0x44332211u;
    constexpr uint32_t high = 0x88776655u;
    const uint32_t expected[] = { 0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u };
    for (uint32_t pass = 0; pass < 8; pass++)
        CHECK(GpuRadixSort::Digit(high, low, pass) == expected[pass]);
}

#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/GpuGeometry.h"

using namespace Crowny;

TEST_CASE("GPU geometry packing preserves meshlet indirect ranges", "[Renderer][Geometry]")
{
    MeshGpuGeometry source;
    source.Lods.push_back({ 0, 0, 0, 1, 0.0f });
    Meshlet meshlet;
    meshlet.TriangleOffset = 3;
    meshlet.TriangleCount = 2;
    meshlet.MaterialSlot = 4;
    source.Meshlets.push_back(meshlet);
    source.MeshletIndices = { 99, 99, 99, 1, 2, 3, 3, 4, 1 };

    PackedGpuGeometry output;
    const uint32_t meshIndex = GpuGeometryPacker::Append(source, 7, 9, 100, output);
    REQUIRE(meshIndex == 0);
    REQUIRE(output.Meshes.size() == 1);
    REQUIRE(output.Lods.size() == 1);
    REQUIRE(output.Meshlets.size() == 1);
    CHECK(output.Meshes[0].LodRangeAndHeaps == glm::uvec4(0, 1, 7, 9));
    CHECK(output.Meshlets[0].Draw == glm::uvec4(3, 6, 4, 0));
    CHECK(output.Meshlets[0].Geometry == glm::uvec4(100, 7, 9, 0));
    CHECK(output.MeshletIndices == source.MeshletIndices);
}

TEST_CASE("GPU geometry table ranges wait for in-flight frames before reuse", "[Renderer][Geometry]")
{
    GpuGeometryTableAllocator allocator(2);
    const GpuGeometryTableRange first = allocator.Allocate(4);
    const GpuGeometryTableRange second = allocator.Allocate(2);
    CHECK(first == (GpuGeometryTableRange{ 0, 4 }));
    CHECK(second == (GpuGeometryTableRange{ 4, 2 }));
    REQUIRE(allocator.Release(first));

    const GpuGeometryTableRange beforeRetirement = allocator.Allocate(3);
    CHECK(beforeRetirement == (GpuGeometryTableRange{ 6, 3 }));
    allocator.BeginFrame(1);
    CHECK(allocator.GetStats().FreeElements == 0);
    allocator.BeginFrame(2);
    CHECK(allocator.GetStats().FreeElements == 4);

    const GpuGeometryTableRange reused = allocator.Allocate(3);
    CHECK(reused == (GpuGeometryTableRange{ 0, 3 }));
    const GpuGeometryTableAllocatorStats stats = allocator.GetStats();
    CHECK(stats.TableSize == 9);
    CHECK(stats.LiveElements == 8);
    CHECK(stats.FreeElements == 1);
    CHECK(stats.HighWaterElements == 8);
}

TEST_CASE("GPU geometry table ranges coalesce adjacent retirements", "[Renderer][Geometry]")
{
    GpuGeometryTableAllocator allocator(1);
    const GpuGeometryTableRange first = allocator.Allocate(2);
    const GpuGeometryTableRange second = allocator.Allocate(3);
    const GpuGeometryTableRange third = allocator.Allocate(1);
    REQUIRE(allocator.Release(second));
    REQUIRE(allocator.Release(first));
    allocator.BeginFrame(1);

    const GpuGeometryTableAllocatorStats stats = allocator.GetStats();
    CHECK(stats.FreeElements == 5);
    CHECK(stats.LargestFreeRange == 5);
    CHECK(stats.RetiredElements == 0);
    CHECK(third == (GpuGeometryTableRange{ 5, 1 }));
}

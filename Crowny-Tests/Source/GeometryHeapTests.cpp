#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/GeometryHeap.h"

using namespace Crowny;

TEST_CASE("Static geometry heaps retire and reuse ranges safely", "[Renderer][Resources][GeometryHeap]")
{
    GeometryHeapDesc desc;
    desc.VertexCapacityBytes = 1024;
    desc.IndexCapacity = 128;
    desc.VertexStride = 16;
    desc.Indices = IndexType::Index_32;
    desc.FramesInFlight = 2;
    StaticGeometryHeap heap(desc);

    GeometryAllocation first;
    GeometryAllocation second;
    REQUIRE(heap.Allocate(64, 6, first));
    REQUIRE(heap.Allocate(64, 6, second));
    CHECK(first.VertexOffsetBytes == 0);
    CHECK(first.VertexCount == 4);
    CHECK(first.FirstIndex == 0);
    CHECK(second.VertexOffsetBytes == 64);

    REQUIRE(heap.Release(first.Handle));
    GeometryAllocation stale;
    CHECK_FALSE(heap.TryGet(first.Handle, stale));
    CHECK_FALSE(heap.Release(first.Handle));
    heap.BeginFrame(1);

    GeometryAllocation beforeRetirement;
    REQUIRE(heap.Allocate(64, 6, beforeRetirement));
    CHECK(beforeRetirement.VertexOffsetBytes == 128);
    heap.BeginFrame(2);

    GeometryAllocation reused;
    REQUIRE(heap.Allocate(64, 6, reused));
    CHECK(reused.VertexOffsetBytes == 0);
    CHECK(reused.Handle.Index == first.Handle.Index);
    CHECK(reused.Handle.Generation != first.Handle.Generation);

    const GeometryHeapStats stats = heap.GetStats();
    CHECK(stats.LiveAllocations == 3);
    CHECK(stats.RetiredAllocations == 0);
    CHECK(stats.HighWaterBytes >= stats.LiveBytes);
}

TEST_CASE("Static geometry heaps coalesce free ranges and report failures", "[Renderer][Resources][GeometryHeap]")
{
    GeometryHeapDesc desc;
    desc.VertexCapacityBytes = 256;
    desc.IndexCapacity = 32;
    desc.VertexStride = 16;
    desc.Indices = IndexType::Index_16;
    desc.FramesInFlight = 1;
    StaticGeometryHeap heap(desc);

    GeometryAllocation first;
    GeometryAllocation second;
    REQUIRE(heap.Allocate(64, 8, first));
    REQUIRE(heap.Allocate(96, 12, second));
    GeometryAllocation invalid;
    CHECK_FALSE(heap.Allocate(15, 1, invalid));

    REQUIRE(heap.Release(first.Handle));
    REQUIRE(heap.Release(second.Handle));
    heap.BeginFrame(1);

    const GeometryHeapStats stats = heap.GetStats();
    CHECK(stats.FreeVertexBytes == 256);
    CHECK(stats.LargestFreeVertexRange == 256);
    CHECK(stats.FreeIndexBytes == 64);
    CHECK(stats.LargestFreeIndexRange == 64);
    CHECK(stats.LiveAllocations == 0);
    CHECK(stats.FailedAllocations == 1);
}

TEST_CASE("Geometry heap layout matching is structural", "[Renderer][Resources][GeometryHeap]")
{
    BufferElement position(ShaderDataType::Float3, VertexAttribute::Position);
    position.Name = "positionA";
    position.Location = 0;
    BufferElement uv(ShaderDataType::Float2, VertexAttribute::TexCoord0);
    uv.Name = "uvA";
    uv.Location = 1;
    BufferLayout first{ position, uv };

    position.Name = "positionB";
    uv.Name = "uvB";
    BufferLayout equivalent{ position, uv };
    CHECK(GeometryLayoutsMatch(first, equivalent));

    uv.Normalized = true;
    BufferLayout normalized{ position, uv };
    CHECK_FALSE(GeometryLayoutsMatch(first, normalized));

    uv.Normalized = false;
    uv.Location = 2;
    BufferLayout differentLocation{ position, uv };
    CHECK_FALSE(GeometryLayoutsMatch(first, differentLocation));
}

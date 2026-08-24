#pragma once

#include "Crowny/RenderAPI/IndexBuffer.h"
#include "Crowny/RenderAPI/VertexBuffer.h"

#include <limits>

namespace Crowny
{
    struct GeometryHeapDesc
    {
        uint32_t VertexCapacityBytes = 64u * 1024u * 1024u;
        uint32_t IndexCapacity = 16u * 1024u * 1024u;
        uint32_t VertexStride = 0;
        IndexType Indices = IndexType::Index_32;
        uint32_t FramesInFlight = 2;
    };

    struct GeometryAllocationHandle
    {
        static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

        uint32_t Index = InvalidIndex;
        uint32_t Generation = 0;

        explicit operator bool() const { return Index != InvalidIndex && Generation != 0; }
        bool operator==(const GeometryAllocationHandle&) const = default;
    };

    struct GeometryAllocation
    {
        GeometryAllocationHandle Handle;
        uint32_t VertexOffsetBytes = 0;
        uint32_t VertexSizeBytes = 0;
        uint32_t VertexOffset = 0;
        uint32_t VertexCount = 0;
        uint32_t FirstIndex = 0;
        uint32_t IndexCount = 0;
    };

    struct GeometryHeapStats
    {
        uint64_t VertexCapacityBytes = 0;
        uint64_t FreeVertexBytes = 0;
        uint64_t LargestFreeVertexRange = 0;
        uint64_t IndexCapacityBytes = 0;
        uint64_t FreeIndexBytes = 0;
        uint64_t LargestFreeIndexRange = 0;
        uint64_t LiveBytes = 0;
        uint64_t HighWaterBytes = 0;
        uint32_t LiveAllocations = 0;
        uint32_t RetiredAllocations = 0;
        uint64_t FailedAllocations = 0;
    };

    // Fixed-capacity static geometry storage. Allocations expose the offsets used
    // by indexed indirect draws. Released ranges remain unavailable until the
    // frames-in-flight retirement window has elapsed.
    class StaticGeometryHeap
    {
    public:
        explicit StaticGeometryHeap(const GeometryHeapDesc& desc);

        bool InitializeGpuBuffers(const Ref<BufferLayout>& layout);
        bool Allocate(uint32_t vertexSizeBytes, uint32_t indexCount, GeometryAllocation& output);
        bool Upload(GeometryAllocationHandle handle, const void* vertexData, const void* indexData);
        bool Release(GeometryAllocationHandle handle);
        void BeginFrame(uint64_t frameNumber);

        bool TryGet(GeometryAllocationHandle handle, GeometryAllocation& output) const;
        GeometryHeapStats GetStats() const;

        const Ref<VertexBuffer>& GetVertexBuffer() const { return m_VertexBuffer; }
        const Ref<IndexBuffer>& GetIndexBuffer() const { return m_IndexBuffer; }
        const GeometryHeapDesc& GetDesc() const { return m_Desc; }

    private:
        struct Range
        {
            uint32_t Offset = 0;
            uint32_t Size = 0;
        };

        struct Slot
        {
            uint32_t Generation = 1;
            bool Alive = false;
            GeometryAllocation Allocation;
        };

        struct RetiredAllocation
        {
            uint32_t SlotIndex = 0;
            Range Vertices;
            Range Indices;
            uint64_t RetiredFrame = 0;
        };

        static bool AllocateRange(Vector<Range>& ranges, uint32_t size, uint32_t alignment, Range& output);
        static void FreeRange(Vector<Range>& ranges, Range range);
        static uint32_t AlignUp(uint32_t value, uint32_t alignment);
        static uint32_t IndexElementSize(IndexType type);
        static uint32_t NextGeneration(uint32_t generation);
        static uint64_t SumRanges(const Vector<Range>& ranges);
        static uint64_t LargestRange(const Vector<Range>& ranges);

        GeometryHeapDesc m_Desc;
        uint64_t m_CurrentFrame = 0;
        Vector<Range> m_FreeVertices;
        Vector<Range> m_FreeIndices;
        Vector<Slot> m_Slots;
        Vector<uint32_t> m_FreeSlots;
        Vector<RetiredAllocation> m_Retired;
        Ref<VertexBuffer> m_VertexBuffer;
        Ref<IndexBuffer> m_IndexBuffer;
        uint64_t m_LiveBytes = 0;
        uint64_t m_HighWaterBytes = 0;
        uint64_t m_FailedAllocations = 0;
        uint32_t m_LiveAllocations = 0;
    };
} // namespace Crowny

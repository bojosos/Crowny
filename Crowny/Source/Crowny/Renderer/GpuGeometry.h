#pragma once

#include "Crowny/Renderer/Mesh.h"

#include <limits>

namespace Crowny
{
    struct alignas(16) GpuMeshRecord
    {
        glm::uvec4 LodRangeAndHeaps = glm::uvec4(0u);
        glm::uvec4 GeometryOffsets = glm::uvec4(0u);
    };

    struct alignas(16) GpuMeshLodData
    {
        uint32_t FirstMeshlet = 0;
        uint32_t MeshletCount = 0;
        float Error = 0.0f;
        uint32_t Padding = 0;
    };

    struct alignas(16) GpuMeshletData
    {
        glm::vec4 BoundingSphere = glm::vec4(0.0f);
        glm::vec4 NormalCone = glm::vec4(0.0f);
        // Expanded index offset/count, material slot, and source LOD.
        glm::uvec4 Draw = glm::uvec4(0u);
        // Vertex base, vertex heap, index heap, and reserved flags.
        glm::uvec4 Geometry = glm::uvec4(0u);
    };

    static_assert(sizeof(GpuMeshRecord) == 32);
    static_assert(sizeof(GpuMeshLodData) == 16);
    static_assert(sizeof(GpuMeshletData) == 64);

    struct PackedGpuGeometry
    {
        Vector<GpuMeshRecord> Meshes;
        Vector<GpuMeshLodData> Lods;
        Vector<GpuMeshletData> Meshlets;
        Vector<uint32_t> MeshletIndices;
    };

    struct GpuGeometryTableRange
    {
        uint32_t First = 0;
        uint32_t Count = 0;

        explicit operator bool() const { return Count != 0; }
        bool operator==(const GpuGeometryTableRange&) const = default;
    };

    struct GpuGeometryTableAllocatorStats
    {
        uint32_t TableSize = 0;
        uint32_t FreeElements = 0;
        uint32_t LargestFreeRange = 0;
        uint32_t LiveElements = 0;
        uint32_t RetiredElements = 0;
        uint32_t HighWaterElements = 0;
    };

    // Grows a sparse GPU metadata table and recycles released ranges only after
    // the frames that could still reference them have retired.
    class GpuGeometryTableAllocator
    {
    public:
        explicit GpuGeometryTableAllocator(uint32_t framesInFlight = 2);

        GpuGeometryTableRange Allocate(uint32_t count);
        bool Release(GpuGeometryTableRange range);
        void BeginFrame(uint64_t frameNumber);
        void Reset();

        GpuGeometryTableAllocatorStats GetStats() const;

    private:
        struct RetiredRange
        {
            GpuGeometryTableRange Range;
            uint64_t Frame = 0;
        };

        static void InsertFreeRange(Vector<GpuGeometryTableRange>& ranges, GpuGeometryTableRange range);

        uint32_t m_FramesInFlight = 2;
        uint32_t m_TableSize = 0;
        uint32_t m_LiveElements = 0;
        uint32_t m_HighWaterElements = 0;
        uint64_t m_CurrentFrame = 0;
        Vector<GpuGeometryTableRange> m_FreeRanges;
        Vector<RetiredRange> m_RetiredRanges;
    };

    class GpuGeometryPacker
    {
    public:
        static uint32_t Append(const MeshGpuGeometry& geometry, uint32_t vertexHeap, uint32_t indexHeap,
                               uint32_t vertexBase, PackedGpuGeometry& output);
    };
} // namespace Crowny

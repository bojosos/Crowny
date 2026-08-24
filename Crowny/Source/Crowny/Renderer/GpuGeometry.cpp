#include "cwpch.h"

#include "Crowny/Renderer/GpuGeometry.h"

namespace Crowny
{
    GpuGeometryTableAllocator::GpuGeometryTableAllocator(uint32_t framesInFlight)
      : m_FramesInFlight(std::max(framesInFlight, 1u))
    {
    }

    GpuGeometryTableRange GpuGeometryTableAllocator::Allocate(uint32_t count)
    {
        if (count == 0)
            return {};

        size_t bestIndex = m_FreeRanges.size();
        uint32_t bestCount = std::numeric_limits<uint32_t>::max();
        for (size_t index = 0; index < m_FreeRanges.size(); index++)
        {
            if (m_FreeRanges[index].Count >= count && m_FreeRanges[index].Count < bestCount)
            {
                bestIndex = index;
                bestCount = m_FreeRanges[index].Count;
            }
        }

        GpuGeometryTableRange result;
        if (bestIndex != m_FreeRanges.size())
        {
            GpuGeometryTableRange& free = m_FreeRanges[bestIndex];
            result = { free.First, count };
            free.First += count;
            free.Count -= count;
            if (free.Count == 0)
                m_FreeRanges.erase(m_FreeRanges.begin() + bestIndex);
        }
        else
        {
            if (count > std::numeric_limits<uint32_t>::max() - m_TableSize)
                return {};
            result = { m_TableSize, count };
            m_TableSize += count;
        }

        m_LiveElements += count;
        m_HighWaterElements = std::max(m_HighWaterElements, m_LiveElements);
        return result;
    }

    bool GpuGeometryTableAllocator::Release(GpuGeometryTableRange range)
    {
        if (!range || range.First >= m_TableSize || range.Count > m_TableSize - range.First ||
            range.Count > m_LiveElements)
            return false;
        m_LiveElements -= range.Count;
        m_RetiredRanges.push_back({ range, m_CurrentFrame });
        return true;
    }

    void GpuGeometryTableAllocator::BeginFrame(uint64_t frameNumber)
    {
        m_CurrentFrame = std::max(m_CurrentFrame, frameNumber);
        for (size_t index = 0; index < m_RetiredRanges.size();)
        {
            const RetiredRange retired = m_RetiredRanges[index];
            if (m_CurrentFrame < retired.Frame + m_FramesInFlight)
            {
                ++index;
                continue;
            }
            InsertFreeRange(m_FreeRanges, retired.Range);
            m_RetiredRanges[index] = m_RetiredRanges.back();
            m_RetiredRanges.pop_back();
        }
    }

    void GpuGeometryTableAllocator::Reset()
    {
        m_TableSize = 0;
        m_LiveElements = 0;
        m_HighWaterElements = 0;
        m_CurrentFrame = 0;
        m_FreeRanges.clear();
        m_RetiredRanges.clear();
    }

    GpuGeometryTableAllocatorStats GpuGeometryTableAllocator::GetStats() const
    {
        GpuGeometryTableAllocatorStats stats;
        stats.TableSize = m_TableSize;
        stats.LiveElements = m_LiveElements;
        stats.HighWaterElements = m_HighWaterElements;
        for (const GpuGeometryTableRange& range : m_FreeRanges)
        {
            stats.FreeElements += range.Count;
            stats.LargestFreeRange = std::max(stats.LargestFreeRange, range.Count);
        }
        for (const RetiredRange& retired : m_RetiredRanges)
            stats.RetiredElements += retired.Range.Count;
        return stats;
    }

    void GpuGeometryTableAllocator::InsertFreeRange(Vector<GpuGeometryTableRange>& ranges,
                                                     GpuGeometryTableRange range)
    {
        if (!range)
            return;
        auto position = std::lower_bound(ranges.begin(), ranges.end(), range.First,
                                         [](const GpuGeometryTableRange& candidate, uint32_t first) {
                                             return candidate.First < first;
                                         });
        position = ranges.insert(position, range);
        if (position != ranges.begin())
        {
            auto previous = position - 1;
            if (previous->First + previous->Count == position->First)
            {
                previous->Count += position->Count;
                position = ranges.erase(position) - 1;
            }
        }
        auto next = position + 1;
        if (next != ranges.end() && position->First + position->Count == next->First)
        {
            position->Count += next->Count;
            ranges.erase(next);
        }
    }

    uint32_t GpuGeometryPacker::Append(const MeshGpuGeometry& geometry, uint32_t vertexHeap, uint32_t indexHeap,
                                       uint32_t vertexBase, PackedGpuGeometry& output)
    {
        const uint32_t meshIndex = static_cast<uint32_t>(output.Meshes.size());
        const uint32_t lodOffset = static_cast<uint32_t>(output.Lods.size());
        const uint32_t meshletOffset = static_cast<uint32_t>(output.Meshlets.size());
        const uint32_t indexOffset = static_cast<uint32_t>(output.MeshletIndices.size());

        GpuMeshRecord mesh;
        mesh.LodRangeAndHeaps = { lodOffset, static_cast<uint32_t>(geometry.Lods.size()), vertexHeap, indexHeap };
        mesh.GeometryOffsets = { vertexBase, indexOffset, meshletOffset, static_cast<uint32_t>(geometry.Meshlets.size()) };
        output.Meshes.push_back(mesh);
        output.MeshletIndices.insert(output.MeshletIndices.end(), geometry.MeshletIndices.begin(), geometry.MeshletIndices.end());

        output.Lods.reserve(output.Lods.size() + geometry.Lods.size());
        for (const MeshLod& source : geometry.Lods)
            output.Lods.push_back({ meshletOffset + source.FirstMeshlet, source.MeshletCount, source.Error, 0u });

        output.Meshlets.reserve(output.Meshlets.size() + geometry.Meshlets.size());
        uint32_t lodIndex = 0;
        for (uint32_t sourceIndex = 0; sourceIndex < geometry.Meshlets.size(); sourceIndex++)
        {
            while (lodIndex + 1u < geometry.Lods.size() &&
                   sourceIndex >= geometry.Lods[lodIndex].FirstMeshlet + geometry.Lods[lodIndex].MeshletCount)
                lodIndex++;
            const Meshlet& source = geometry.Meshlets[sourceIndex];
            GpuMeshletData meshlet;
            meshlet.BoundingSphere = source.BoundingSphere;
            meshlet.NormalCone = source.NormalCone;
            meshlet.Draw = { indexOffset + source.TriangleOffset, source.TriangleCount * 3u,
                             source.MaterialSlot, lodIndex };
            meshlet.Geometry = { vertexBase, vertexHeap, indexHeap, 0u };
            output.Meshlets.push_back(meshlet);
        }
        return meshIndex;
    }
} // namespace Crowny

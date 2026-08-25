#include "cwpch.h"

#include "Crowny/Renderer/GeometryHeap.h"

namespace Crowny
{
    StaticGeometryHeap::StaticGeometryHeap(const GeometryHeapDesc& desc) : m_Desc(desc)
    {
        m_Desc.FramesInFlight = std::max(m_Desc.FramesInFlight, 1u);
        if (m_Desc.VertexCapacityBytes > 0)
            m_FreeVertices.push_back({ 0u, m_Desc.VertexCapacityBytes });
        if (m_Desc.IndexCapacity > 0)
            m_FreeIndices.push_back({ 0u, m_Desc.IndexCapacity });
    }

    bool StaticGeometryHeap::InitializeGpuBuffers(const Ref<BufferLayout>& layout)
    {
        if (m_Desc.VertexCapacityBytes == 0 || m_Desc.IndexCapacity == 0 || m_Desc.VertexStride == 0)
            return false;
        VertexBufferDesc vertexDesc;
        vertexDesc.Size = m_Desc.VertexCapacityBytes;
        vertexDesc.Usage = BufferUsage::BU_STATIC_DRAW;
        IndexBufferDesc indexDesc;
        indexDesc.Count = m_Desc.IndexCapacity;
        indexDesc.Type = m_Desc.Indices;
        indexDesc.Usage = BufferUsage::BU_STATIC_DRAW;
        Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(vertexDesc);
        Ref<IndexBuffer> indexBuffer = IndexBuffer::Create(indexDesc);
        if (!vertexBuffer || !indexBuffer)
            return false;
        vertexBuffer->SetLayout(layout);
        m_VertexBuffer = std::move(vertexBuffer);
        m_IndexBuffer = std::move(indexBuffer);
        return true;
    }

    bool StaticGeometryHeap::Allocate(uint32_t vertexSizeBytes, uint32_t indexCount, GeometryAllocation& output)
    {
        output = {};
        if (vertexSizeBytes == 0 || indexCount == 0 || m_Desc.VertexStride == 0 || vertexSizeBytes % m_Desc.VertexStride != 0)
        {
            ++m_FailedAllocations;
            return false;
        }

        Range vertices;
        if (!AllocateRange(m_FreeVertices, vertexSizeBytes, m_Desc.VertexStride, vertices))
        {
            ++m_FailedAllocations;
            return false;
        }
        Range indices;
        if (!AllocateRange(m_FreeIndices, indexCount, 1u, indices))
        {
            FreeRange(m_FreeVertices, vertices);
            ++m_FailedAllocations;
            return false;
        }

        uint32_t slotIndex;
        if (m_FreeSlots.empty())
        {
            slotIndex = static_cast<uint32_t>(m_Slots.size());
            m_Slots.emplace_back();
        }
        else
        {
            slotIndex = m_FreeSlots.back();
            m_FreeSlots.pop_back();
        }
        Slot& slot = m_Slots[slotIndex];
        slot.Alive = true;
        slot.Allocation.Handle = { slotIndex, slot.Generation };
        slot.Allocation.VertexOffsetBytes = vertices.Offset;
        slot.Allocation.VertexSizeBytes = vertices.Size;
        slot.Allocation.VertexOffset = vertices.Offset / m_Desc.VertexStride;
        slot.Allocation.VertexCount = vertices.Size / m_Desc.VertexStride;
        slot.Allocation.FirstIndex = indices.Offset;
        slot.Allocation.IndexCount = indices.Size;
        output = slot.Allocation;

        const uint64_t indexBytes = static_cast<uint64_t>(indices.Size) * IndexElementSize(m_Desc.Indices);
        m_LiveBytes += vertices.Size + indexBytes;
        m_HighWaterBytes = std::max(m_HighWaterBytes, m_LiveBytes);
        ++m_LiveAllocations;
        return true;
    }

    bool StaticGeometryHeap::Upload(GeometryAllocationHandle handle, const void* vertexData, const void* indexData)
    {
        if (!vertexData || !indexData)
            return false;
        return UploadVertices(handle, vertexData) && UploadIndices(handle, indexData);
    }

    bool StaticGeometryHeap::UploadVertices(GeometryAllocationHandle handle, const void* vertexData)
    {
        GeometryAllocation allocation;
        if (!vertexData || !m_VertexBuffer || !TryGet(handle, allocation))
            return false;
        m_VertexBuffer->WriteData(allocation.VertexOffsetBytes, allocation.VertexSizeBytes, vertexData);
        return true;
    }

    bool StaticGeometryHeap::UploadIndices(GeometryAllocationHandle handle, const void* indexData)
    {
        GeometryAllocation allocation;
        if (!indexData || !m_IndexBuffer || !TryGet(handle, allocation))
            return false;
        const uint32_t indexSize = IndexElementSize(m_Desc.Indices);
        m_IndexBuffer->WriteData(allocation.FirstIndex * indexSize, allocation.IndexCount * indexSize, indexData);
        return true;
    }

    bool StaticGeometryHeap::CopyVertices(GeometryAllocationHandle handle, VertexBuffer& source, uint32_t sourceOffsetBytes)
    {
        GeometryAllocation allocation;
        if (!m_VertexBuffer || !m_VertexBuffer->GetLayout() || !source.GetLayout() ||
            !GeometryLayoutsMatch(*m_VertexBuffer->GetLayout(), *source.GetLayout()) || !TryGet(handle, allocation) ||
            sourceOffsetBytes > source.GetBufferSize() || allocation.VertexSizeBytes > source.GetBufferSize() - sourceOffsetBytes)
            return false;
        m_VertexBuffer->CopyData(source, sourceOffsetBytes, allocation.VertexOffsetBytes, allocation.VertexSizeBytes);
        return true;
    }

    bool StaticGeometryHeap::CopyIndices(GeometryAllocationHandle handle, IndexBuffer& source, uint32_t sourceFirstIndex)
    {
        GeometryAllocation allocation;
        if (!m_IndexBuffer || source.GetIndexType() != m_Desc.Indices || !TryGet(handle, allocation) || sourceFirstIndex > source.GetCount() ||
            allocation.IndexCount > source.GetCount() - sourceFirstIndex)
            return false;
        const uint32_t indexSize = IndexElementSize(m_Desc.Indices);
        m_IndexBuffer->CopyData(source, sourceFirstIndex * indexSize, allocation.FirstIndex * indexSize, allocation.IndexCount * indexSize);
        return true;
    }

    bool StaticGeometryHeap::Release(GeometryAllocationHandle handle)
    {
        if (!handle || handle.Index >= m_Slots.size())
            return false;
        Slot& slot = m_Slots[handle.Index];
        if (!slot.Alive || slot.Generation != handle.Generation)
            return false;
        slot.Alive = false;
        m_Retired.push_back({ handle.Index,
                              { slot.Allocation.VertexOffsetBytes, slot.Allocation.VertexSizeBytes },
                              { slot.Allocation.FirstIndex, slot.Allocation.IndexCount },
                              m_CurrentFrame });
        const uint64_t indexBytes = static_cast<uint64_t>(slot.Allocation.IndexCount) * IndexElementSize(m_Desc.Indices);
        m_LiveBytes -= slot.Allocation.VertexSizeBytes + indexBytes;
        --m_LiveAllocations;
        return true;
    }

    void StaticGeometryHeap::BeginFrame(uint64_t frameNumber)
    {
        m_CurrentFrame = std::max(m_CurrentFrame, frameNumber);
        for (size_t index = 0; index < m_Retired.size();)
        {
            const RetiredAllocation& retired = m_Retired[index];
            if (m_CurrentFrame < retired.RetiredFrame + m_Desc.FramesInFlight)
            {
                ++index;
                continue;
            }
            FreeRange(m_FreeVertices, retired.Vertices);
            FreeRange(m_FreeIndices, retired.Indices);
            Slot& slot = m_Slots[retired.SlotIndex];
            slot.Generation = NextGeneration(slot.Generation);
            slot.Allocation = {};
            m_FreeSlots.push_back(retired.SlotIndex);
            m_Retired[index] = m_Retired.back();
            m_Retired.pop_back();
        }
    }

    bool StaticGeometryHeap::TryGet(GeometryAllocationHandle handle, GeometryAllocation& output) const
    {
        if (!handle || handle.Index >= m_Slots.size())
            return false;
        const Slot& slot = m_Slots[handle.Index];
        if (!slot.Alive || slot.Generation != handle.Generation)
            return false;
        output = slot.Allocation;
        return true;
    }

    GeometryHeapStats StaticGeometryHeap::GetStats() const
    {
        GeometryHeapStats stats;
        stats.VertexCapacityBytes = m_Desc.VertexCapacityBytes;
        stats.FreeVertexBytes = SumRanges(m_FreeVertices);
        stats.LargestFreeVertexRange = LargestRange(m_FreeVertices);
        const uint64_t indexSize = IndexElementSize(m_Desc.Indices);
        stats.IndexCapacityBytes = static_cast<uint64_t>(m_Desc.IndexCapacity) * indexSize;
        stats.FreeIndexBytes = SumRanges(m_FreeIndices) * indexSize;
        stats.LargestFreeIndexRange = LargestRange(m_FreeIndices) * indexSize;
        stats.LiveBytes = m_LiveBytes;
        stats.HighWaterBytes = m_HighWaterBytes;
        stats.LiveAllocations = m_LiveAllocations;
        stats.RetiredAllocations = static_cast<uint32_t>(m_Retired.size());
        stats.FailedAllocations = m_FailedAllocations;
        return stats;
    }

    bool StaticGeometryHeap::AllocateRange(Vector<Range>& ranges, uint32_t size, uint32_t alignment, Range& output)
    {
        size_t bestIndex = ranges.size();
        uint32_t bestOffset = 0;
        uint64_t bestWaste = std::numeric_limits<uint64_t>::max();
        for (size_t index = 0; index < ranges.size(); ++index)
        {
            const Range& range = ranges[index];
            const uint32_t alignedOffset = AlignUp(range.Offset, alignment);
            if (alignedOffset < range.Offset || alignedOffset - range.Offset > range.Size || size > range.Size - (alignedOffset - range.Offset))
                continue;
            const uint64_t waste = static_cast<uint64_t>(range.Size) - size;
            if (waste < bestWaste)
            {
                bestIndex = index;
                bestOffset = alignedOffset;
                bestWaste = waste;
            }
        }
        if (bestIndex == ranges.size())
            return false;

        const Range original = ranges[bestIndex];
        ranges.erase(ranges.begin() + bestIndex);
        const uint32_t leading = bestOffset - original.Offset;
        const uint32_t allocationEnd = bestOffset + size;
        const uint32_t originalEnd = original.Offset + original.Size;
        if (leading > 0)
            FreeRange(ranges, { original.Offset, leading });
        if (allocationEnd < originalEnd)
            FreeRange(ranges, { allocationEnd, originalEnd - allocationEnd });
        output = { bestOffset, size };
        return true;
    }

    void StaticGeometryHeap::FreeRange(Vector<Range>& ranges, Range range)
    {
        if (range.Size == 0)
            return;
        auto position = std::lower_bound(ranges.begin(), ranges.end(), range.Offset,
                                         [](const Range& candidate, uint32_t offset) { return candidate.Offset < offset; });
        position = ranges.insert(position, range);
        if (position != ranges.begin())
        {
            auto previous = position - 1;
            if (previous->Offset + previous->Size == position->Offset)
            {
                previous->Size += position->Size;
                position = ranges.erase(position) - 1;
            }
        }
        auto next = position + 1;
        if (next != ranges.end() && position->Offset + position->Size == next->Offset)
        {
            position->Size += next->Size;
            ranges.erase(next);
        }
    }

    uint32_t StaticGeometryHeap::AlignUp(uint32_t value, uint32_t alignment)
    {
        if (alignment <= 1u)
            return value;
        const uint32_t remainder = value % alignment;
        if (remainder == 0u)
            return value;
        const uint32_t padding = alignment - remainder;
        return value > std::numeric_limits<uint32_t>::max() - padding ? std::numeric_limits<uint32_t>::max() : value + padding;
    }

    uint32_t StaticGeometryHeap::IndexElementSize(IndexType type) { return type == IndexType::Index_16 ? sizeof(uint16_t) : sizeof(uint32_t); }

    uint32_t StaticGeometryHeap::NextGeneration(uint32_t generation)
    {
        ++generation;
        return generation == 0 ? 1u : generation;
    }

    uint64_t StaticGeometryHeap::SumRanges(const Vector<Range>& ranges)
    {
        uint64_t total = 0;
        for (const Range& range : ranges)
            total += range.Size;
        return total;
    }

    uint64_t StaticGeometryHeap::LargestRange(const Vector<Range>& ranges)
    {
        uint64_t largest = 0;
        for (const Range& range : ranges)
            largest = std::max<uint64_t>(largest, range.Size);
        return largest;
    }

    bool GeometryLayoutsMatch(const BufferLayout& first, const BufferLayout& second)
    {
        if (first.GetStreamCount() != second.GetStreamCount())
            return false;
        for (uint32_t stream = 0; stream < first.GetStreamCount(); stream++)
            if (first.GetStride(stream) != second.GetStride(stream))
                return false;
        const auto& firstElements = first.GetElements();
        const auto& secondElements = second.GetElements();
        if (firstElements.size() != secondElements.size())
            return false;
        for (size_t index = 0; index < firstElements.size(); index++)
        {
            const BufferElement& a = firstElements[index];
            const BufferElement& b = secondElements[index];
            if (a.Attribute != b.Attribute || a.Type != b.Type || a.Size != b.Size || a.Offset != b.Offset || a.StreamIdx != b.StreamIdx ||
                a.InstanceRate != b.InstanceRate || a.Location != b.Location || a.Normalized != b.Normalized)
                return false;
        }
        return true;
    }
} // namespace Crowny

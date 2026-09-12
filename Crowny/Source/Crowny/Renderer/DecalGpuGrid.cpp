#include "cwpch.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/RenderAPI/GenericGpuBuffer.h"
#include "Crowny/RenderAPI/Query.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/RenderCapabilities.h"
#include "Crowny/Renderer/DecalGpuGrid.h"

namespace Crowny
{
    bool DecalGpuGrid::GetStatistics(uint64_t viewId, DecalGridStatistics& statistics)
    {
        for (auto entry = m_PendingStatistics.begin(); entry != m_PendingStatistics.end();)
        {
            glm::uvec4 counters{};
            if (!entry->Timer->IsReady() || !entry->Readback->TryRead(&counters, sizeof(counters)))
            {
                ++entry;
                continue;
            }
            auto& latest = m_Statistics[entry->ViewId];
            if (entry->FrameNumber >= latest.FrameNumber)
                latest = { entry->FrameNumber, counters.y, counters.z, counters.w, entry->Timer->GetTimeMs() };
            entry = m_PendingStatistics.erase(entry);
        }
        const auto found = m_Statistics.find(viewId);
        if (found == m_Statistics.end())
            return false;
        statistics = found->second;
        return true;
    }

    void DecalGpuGrid::ReleaseView(uint64_t viewId)
    {
        m_Statistics.erase(viewId);
        std::erase_if(m_PendingStatistics, [viewId](const auto& entry) { return entry.ViewId == viewId; });
    }

    bool DecalGpuGrid::MatchesReference(const Vector<glm::uvec2>& cells, const Vector<uint32_t>& indices) const
    {
        // ReadData waits for submitted GPU writes. The grid dispatches are still
        // in the current command buffer, so submit them before validation reads.
        RenderAPI::Get().SubmitCommandBuffer(nullptr);
        Vector<glm::uvec2> actualCells(cells.size());
        Vector<uint32_t> actualIndices(std::min(indices.size(), size_t(m_Indices->GetBufferSize() / sizeof(uint32_t))));
        if (!actualCells.empty())
            m_Cells->ReadData(0, static_cast<uint32_t>(actualCells.size() * sizeof(glm::uvec2)), actualCells.data());
        if (!actualIndices.empty())
            m_Indices->ReadData(0, static_cast<uint32_t>(actualIndices.size() * sizeof(uint32_t)), actualIndices.data());
        for (size_t i = 0; i < cells.size(); ++i)
        {
            // A device-memory capacity overflow may also select the complete
            // visible list. It is correct even if the reference fits 64 entries.
            if (actualCells[i].y == UINT32_MAX && (cells[i].y == UINT32_MAX || indices.size() > actualIndices.size()))
                continue;
            if (cells[i].y != actualCells[i].y)
            {
                CW_ENGINE_ERROR("Decal grid cell {}: expected count {}, GPU count {} (offset {})", i, cells[i].y, actualCells[i].y, actualCells[i].x);
                return false;
            }
            if (cells[i].y == UINT32_MAX)
                continue;
            if (uint64_t(actualCells[i].x) + actualCells[i].y > actualIndices.size())
            {
                CW_ENGINE_ERROR("Decal grid cell {}: range {} + {} exceeds readback size {}", i, actualCells[i].x, actualCells[i].y,
                                actualIndices.size());
                return false;
            }
            for (uint32_t index = 0; index < cells[i].y; ++index)
                if (indices[cells[i].x + index] != actualIndices[actualCells[i].x + index])
                    return false;
        }
        return true;
    }

    bool DecalGpuGrid::Build(const ClusteredLightGridDesc& desc, const glm::mat4& view, const glm::mat4& projection, const Vector<glm::vec4>& bounds,
                             uint64_t viewId, uint64_t frameNumber)
    {
        if (bounds.empty())
            return false;
        if (!m_Builder.IsValid() && !m_Builder.Initialize(AssetManager::Get().Load<Shader>("Resources/Shaders/BuildDecalGrid.asset")))
            return false;
        const auto dimensions = ClusteredLightBuilder::GetDimensions(desc);
        const uint32_t cellCount = dimensions.x * dimensions.y * dimensions.z;
        // Reserve without constructing a second cluster grid on the CPU. Bound
        // memory at 64 MiB; exhausted allocations use the same complete-list
        // fallback as clusters with more than 64 candidates.
        const uint64_t deviceLimit = RenderAPI::Get().GetCapabilities().MaxStorageBufferRange;
        const uint64_t maxBytes = deviceLimit ? std::min(deviceLimit, uint64_t(64 * 1024 * 1024)) : 64 * 1024 * 1024;
        const uint32_t indexCapacity =
          static_cast<uint32_t>(std::min(uint64_t(cellCount) * std::min(bounds.size(), size_t(64)), maxBytes / sizeof(uint32_t)));
        const auto ensure = [](Ref<GenericGpuBuffer>& buffer, uint32_t count, uint32_t stride, BufferUsage usage) {
            const uint64_t bytes = uint64_t(std::max(count, 1u)) * stride;
            if (bytes > UINT32_MAX)
                return false;
            if (!buffer || buffer->GetBufferSize() < bytes)
            {
                GenericGpuBufferDesc allocation;
                allocation.ElementCount = std::max(count, 1u);
                allocation.ElementSize = stride;
                allocation.Type = GpuBufferType::Structured;
                allocation.Usage = usage;
                buffer = GenericGpuBuffer::Create(allocation);
            }
            return buffer != nullptr;
        };
        // Shader-written buffers must participate in dispatch-to-dispatch barriers.
        if (!ensure(m_Cells, cellCount, sizeof(glm::uvec2), BufferUsage::BU_LOADSTORE) ||
            !ensure(m_Indices, indexCapacity, sizeof(uint32_t), BufferUsage::BU_LOADSTORE) ||
            !ensure(m_Bounds, static_cast<uint32_t>(bounds.size()), sizeof(glm::vec4), BufferUsage::BU_DYNAMIC_DRAW) ||
            !ensure(m_Ranges, static_cast<uint32_t>(bounds.size()), sizeof(glm::uvec4) * 2, BufferUsage::BU_LOADSTORE) ||
            !ensure(m_Cursors, cellCount, sizeof(uint32_t), BufferUsage::BU_LOADSTORE) ||
            !ensure(m_Counters, 4, sizeof(uint32_t), BufferUsage::BU_LOADSTORE))
            return false;
        m_Bounds->WriteData(0, static_cast<uint32_t>(bounds.size() * sizeof(glm::vec4)), bounds.data(), BWT_DISCARD);
        struct alignas(16) Constants
        {
            glm::mat4 View, Projection;
            glm::vec4 DepthViewport;
            glm::uvec4 Dimensions;
            glm::uvec4 Counts;
        } constants{ view,
                     projection,
                     { desc.NearPlane, desc.FarPlane, desc.ViewportWidth, desc.ViewportHeight },
                     { dimensions, desc.TileSize },
                     { bounds.size(), cellCount, indexCapacity, 0 } };
        m_Builder.SetBuffer(0, 1, m_Bounds);
        m_Builder.SetBuffer(0, 2, m_Ranges);
        m_Builder.SetBuffer(0, 3, m_Cells);
        m_Builder.SetBuffer(0, 4, m_Indices);
        m_Builder.SetBuffer(0, 5, m_Cursors);
        m_Builder.SetBuffer(0, 6, m_Counters);
        const Ref<TimerQuery> timer = m_PendingStatistics.size() < 8 ? TimerQuery::Create() : nullptr;
        if (timer)
            timer->Begin();
        for (uint32_t stage = 0; stage < 5; ++stage)
        {
            constants.Counts.w = stage;
            m_Builder.WriteUniformBlock(0, 0, &constants, sizeof(constants));
            const uint32_t work = stage == 1 || stage == 3 ? static_cast<uint32_t>(bounds.size()) : cellCount;
            // Resource access tracking inserts the storage barriers between these dispatches,
            // as it does between the existing Hi-Z mip dispatches.
            if (!m_Builder.Dispatch(stage == 4 ? std::min(cellCount, 65535u) : (work + 63u) / 64u, stage == 4 ? (cellCount + 65534u) / 65535u : 1u))
            {
                if (timer)
                    timer->End();
                return false;
            }
        }
        if (timer)
            timer->End();
        // Keep the queue bounded if GPU work has not retired. Rendering never
        // waits for diagnostics, and each copy retains its own camera identity.
        if (timer)
            if (auto readback = m_Counters->QueueReadback(0, sizeof(glm::uvec4)))
                m_PendingStatistics.push_back({ viewId, frameNumber, std::move(readback), timer });
        return true;
    }
} // namespace Crowny

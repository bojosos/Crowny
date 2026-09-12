#pragma once

#include "Crowny/RenderAPI/GenericGpuBuffer.h"
#include "Crowny/Renderer/ClusteredLightGrid.h"
#include "Crowny/Renderer/ComputeMaterial.h"

namespace Crowny
{
    class TimerQuery;

    struct DecalGridStatistics
    {
        uint64_t FrameNumber = 0;
        uint32_t Overflow = 0;
        uint32_t MaxCandidates = 0;
        uint32_t OccupiedCells = 0;
        float GpuMilliseconds = 0;
    };

    class DecalGpuGrid
    {
    public:
        bool Build(const ClusteredLightGridDesc& desc, const glm::mat4& view, const glm::mat4& projection, const Vector<glm::vec4>& bounds,
                   uint64_t viewId, uint64_t frameNumber);
        const Ref<GenericGpuBuffer>& GetCells() const { return m_Cells; }
        const Ref<GenericGpuBuffer>& GetIndices() const { return m_Indices; }
        bool MatchesReference(const Vector<glm::uvec2>& cells, const Vector<uint32_t>& indices) const;
        bool GetStatistics(uint64_t viewId, DecalGridStatistics& statistics);
        void ReleaseView(uint64_t viewId);

    private:
        ComputeMaterial m_Builder;
        Ref<GenericGpuBuffer> m_Cells, m_Indices, m_Bounds, m_Ranges, m_Cursors, m_Counters;
        struct PendingStatistics
        {
            uint64_t ViewId, FrameNumber;
            Ref<GpuBufferReadback> Readback;
            Ref<TimerQuery> Timer;
        };
        Vector<PendingStatistics> m_PendingStatistics;
        UnorderedMap<uint64_t, DecalGridStatistics> m_Statistics;
    };
} // namespace Crowny

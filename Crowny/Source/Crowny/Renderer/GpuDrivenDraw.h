#pragma once

#include "Crowny/Common/Types.h"
#include "Crowny/RenderAPI/GenericGpuBuffer.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/Renderer/RenderTypes.h"

namespace Crowny
{
    enum class RenderDrawPhase : uint8_t
    {
        Depth,
        Shadow,
        Opaque,
        ForwardOpaque,
        Transparent,
        Overlay
    };

    struct GpuDrawBinKey
    {
        RenderDrawPhase Phase = RenderDrawPhase::Opaque;
        AlphaMode Alpha = AlphaMode::Opaque;
        uint32_t Pipeline = 0;
        uint32_t GeometryHeap = 0;
        uint32_t MaterialTemplate = 0;

        bool operator==(const GpuDrawBinKey& other) const = default;
    };

    // A visible submesh before binning. Material record indices remain in the
    // persistent instance table; only the immutable template participates in
    // the bin key, so different material instances can share a multi-draw.
    struct GpuDrawCandidate
    {
        GpuDrawBinKey Bin;
        uint32_t InstanceID = 0;
        uint32_t MaterialIndex = 0;
        uint32_t IndexCount = 0;
        uint32_t FirstIndex = 0;
        int32_t VertexOffset = 0;
        int32_t RenderLayer = 0;
        float ViewDepth = 0.0f;
    };

    struct GpuVisibleDrawInstance
    {
        uint32_t InstanceID = 0;
        uint32_t MaterialIndex = 0;

        bool operator==(const GpuVisibleDrawInstance& other) const = default;
    };

    struct GpuDrawRun
    {
        GpuDrawBinKey Bin;
        uint32_t FirstCommand = 0;
        uint32_t CommandCount = 0;
    };

    struct GpuDrawList
    {
        Vector<GpuVisibleDrawInstance> Instances;
        Vector<DrawIndexedIndirectCommand> Commands;
        Vector<GpuDrawRun> Runs;
        uint32_t StrictTransparentCommandCount = 0;

        void Clear()
        {
            Instances.clear();
            Commands.clear();
            Runs.clear();
            StrictTransparentCommandCount = 0;
        }
    };

    // Deterministic CPU reference and compatibility path for the same
    // compaction/binning contract used by the Vulkan compute pipeline.
    class GpuDrawListBuilder
    {
    public:
        void Reserve(uint32_t candidateCount, uint32_t commandCount = 0);
        void Build(const GpuDrawCandidate* candidates, uint32_t candidateCount, GpuDrawList& output);

    private:
        struct SortEntry
        {
            GpuDrawCandidate Candidate;
            uint32_t StableIndex = 0;
        };

        static bool IsStrictTransparent(AlphaMode alpha);
        static bool IsOrderIndependent(AlphaMode alpha);
        static bool SameGeometry(const GpuDrawCandidate& first, const GpuDrawCandidate& second);
        static bool BinLess(const GpuDrawBinKey& first, const GpuDrawBinKey& second);

        Vector<SortEntry> m_OrderIndependent;
        Vector<SortEntry> m_StrictTransparent;
    };

    struct GpuDrawBufferStats
    {
        uint64_t UploadedBytes = 0;
        uint32_t InstanceCapacity = 0;
        uint32_t CommandCapacity = 0;
        uint32_t RunCapacity = 0;
    };

    // Persistent buffers used by CPU fallback draw generation. On the fully
    // GPU-driven path these same buffers are written by compute instead.
    class GpuDrawBuffers
    {
    public:
        explicit GpuDrawBuffers(bool enableGpuBuffers = true) : m_EnableGpuBuffers(enableGpuBuffers) {}

        void Upload(const GpuDrawList& list);
        void Reset();

        const Ref<GenericGpuBuffer>& GetInstanceIDBuffer() const { return m_InstanceIDs; }
        const Ref<GenericGpuBuffer>& GetCommandBuffer() const { return m_Commands; }
        const Ref<GenericGpuBuffer>& GetCountBuffer() const { return m_Counts; }
        const GpuDrawBufferStats& GetStats() const { return m_Stats; }
        bool HasGpuBuffers() const { return m_InstanceIDs != nullptr && m_Commands != nullptr && m_Counts != nullptr; }

    private:
        bool CanCreateGpuBuffers() const;
        void EnsureCapacity(uint32_t instanceCount, uint32_t commandCount, uint32_t runCount);

        bool m_EnableGpuBuffers = true;
        Ref<GenericGpuBuffer> m_InstanceIDs;
        Ref<GenericGpuBuffer> m_Commands;
        Ref<GenericGpuBuffer> m_Counts;
        GpuDrawBufferStats m_Stats;
    };
} // namespace Crowny

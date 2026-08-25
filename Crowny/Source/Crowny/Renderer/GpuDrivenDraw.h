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

    struct GpuDrawBinLayoutDesc
    {
        uint32_t MaximumCommands = 0;
        uint32_t MaximumBins = 0;
        uint32_t MaximumDrawsPerCall = 0;

        bool operator==(const GpuDrawBinLayoutDesc&) const = default;
    };

    struct GpuDrawBin
    {
        GpuDrawBinKey Key;
        uint32_t FirstCommand = 0;
        uint32_t CommandCapacity = 0;
        uint32_t CountIndex = 0;

        bool operator==(const GpuDrawBin&) const = default;
    };

    // Open-addressed hash entry consumed directly by the compaction shader.
    // BinIndex == InvalidBin marks an unused lookup slot.
    struct alignas(16) GpuDrawBinLookupEntry
    {
        static constexpr uint32_t InvalidBin = 0xffffffffu;

        uint32_t Phase = 0;
        uint32_t Alpha = 0;
        uint32_t Pipeline = 0;
        uint32_t GeometryHeap = 0;
        uint32_t MaterialTemplate = 0;
        uint32_t BinIndex = InvalidBin;
        uint32_t FirstCommand = 0;
        uint32_t CommandCapacity = 0;

        bool operator==(const GpuDrawBinLookupEntry&) const = default;
    };

    static_assert(sizeof(GpuDrawBinLookupEntry) == 32);

    struct GpuDrawBinLayoutStats
    {
        uint32_t SourceBinCount = 0;
        uint32_t ActiveBinCount = 0;
        uint32_t RejectedBinCount = 0;
        uint32_t CommandCapacity = 0;
        uint32_t LookupCapacity = 0;
        uint64_t Version = 0;
    };

    // Persistent CPU-known submission layout. Compute writes each visible draw
    // into a fixed bin segment, allowing the CPU to bind a heap/pipeline and use
    // DrawIndexedIndirectCount without reading visibility or run counts back.
    class GpuDrawBinLayout
    {
    public:
        bool Build(const GpuDrawBinKey* keys, uint32_t keyCount, const GpuDrawBinLayoutDesc& desc);
        void Reset();

        uint32_t FindBin(const GpuDrawBinKey& key) const;
        bool Contains(const GpuDrawBinKey& key) const { return FindBin(key) != GpuDrawBinLookupEntry::InvalidBin; }
        bool Matches(const GpuDrawBinLayoutDesc& desc) const { return m_Desc == desc; }

        const Vector<GpuDrawBin>& GetBins() const { return m_Bins; }
        const Vector<GpuDrawBinLookupEntry>& GetLookupEntries() const { return m_LookupEntries; }
        const GpuDrawBinLayoutStats& GetStats() const { return m_Stats; }
        uint32_t GetLookupMask() const { return m_LookupEntries.empty() ? 0u : static_cast<uint32_t>(m_LookupEntries.size() - 1u); }

    private:
        static bool BinLess(const GpuDrawBinKey& first, const GpuDrawBinKey& second);
        static uint32_t Hash(const GpuDrawBinKey& key);

        GpuDrawBinLayoutDesc m_Desc;
        Vector<GpuDrawBin> m_Bins;
        Vector<GpuDrawBinLookupEntry> m_LookupEntries;
        GpuDrawBinLayoutStats m_Stats;
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
        Vector<uint32_t> m_RunCounts;
        GpuDrawBufferStats m_Stats;
    };
} // namespace Crowny

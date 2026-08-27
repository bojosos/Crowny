#include "cwpch.h"

#include "Crowny/Renderer/GpuDrivenDraw.h"

#include "Crowny/RenderAPI/RenderCapabilities.h"

#include <bit>

namespace Crowny
{
    namespace
    {
        template <typename T> bool CompareValue(const T& first, const T& second) { return first < second; }

        uint32_t NextCapacity(uint32_t required, uint32_t minimum) { return std::max(std::bit_ceil(std::max(required, 1u)), minimum); }

        template <typename T> void HashValue(uint32_t& hash, T value)
        {
            hash ^= static_cast<uint32_t>(value);
            hash *= 16777619u;
        }

        Ref<GenericGpuBuffer> CreateBuffer(uint32_t elementCount, uint32_t elementSize, GpuBufferType type)
        {
            GenericGpuBufferDesc desc;
            desc.ElementCount = elementCount;
            desc.ElementSize = elementSize;
            desc.Type = type;
            desc.Usage = BufferUsage::BU_LOADSTORE;
            return GenericGpuBuffer::Create(desc);
        }
    } // namespace

    bool GpuDrawBinLayout::BinLess(const GpuDrawBinKey& first, const GpuDrawBinKey& second)
    {
        if (first.Phase != second.Phase)
            return CompareValue(first.Phase, second.Phase);
        if (first.Alpha != second.Alpha)
            return CompareValue(first.Alpha, second.Alpha);
        if (first.Pipeline != second.Pipeline)
            return first.Pipeline < second.Pipeline;
        if (first.GeometryHeap != second.GeometryHeap)
            return first.GeometryHeap < second.GeometryHeap;
        return first.MaterialTemplate < second.MaterialTemplate;
    }

    uint32_t GpuDrawBinLayout::Hash(const GpuDrawBinKey& key)
    {
        uint32_t hash = 2166136261u;
        HashValue(hash, key.Phase);
        HashValue(hash, key.Alpha);
        HashValue(hash, key.Pipeline);
        HashValue(hash, key.GeometryHeap);
        HashValue(hash, key.MaterialTemplate);
        hash ^= hash >> 16u;
        return hash;
    }

    bool GpuDrawBinLayout::Build(const GpuDrawBinKey* keys, uint32_t keyCount, const GpuDrawBinLayoutDesc& desc)
    {
        Vector<GpuDrawBinKey> sourceKeys;
        sourceKeys.reserve(keyCount);
        for (uint32_t index = 0; keys != nullptr && index < keyCount; index++)
            sourceKeys.push_back(keys[index]);
        std::sort(sourceKeys.begin(), sourceKeys.end(), BinLess);
        Vector<GpuDrawBinKey> uniqueKeys;
        Vector<uint32_t> binDemand;
        uniqueKeys.reserve(sourceKeys.size());
        binDemand.reserve(sourceKeys.size());
        for (const GpuDrawBinKey& key : sourceKeys)
        {
            if (uniqueKeys.empty() || !(uniqueKeys.back() == key))
            {
                uniqueKeys.push_back(key);
                binDemand.push_back(1u);
            }
            else
                binDemand.back()++;
        }

        const uint32_t sourceBinCount = static_cast<uint32_t>(uniqueKeys.size());
        Vector<GpuDrawBin> bins;
        Vector<GpuDrawBinLookupEntry> lookupEntries;
        uint32_t commandCapacity = 0;
        if (sourceBinCount != 0 && desc.MaximumBins != 0 && desc.MaximumCommands != 0 && desc.MaximumDrawsPerCall != 0)
        {
            Vector<uint32_t> admissionOrder;
            admissionOrder.reserve(sourceBinCount);
            for (uint32_t index = 0; index < sourceBinCount; index++)
                admissionOrder.push_back(index);
            std::sort(admissionOrder.begin(), admissionOrder.end(), [&](uint32_t first, uint32_t second) {
                if (binDemand[first] != binDemand[second])
                    return binDemand[first] > binDemand[second];
                return first < second;
            });

            Vector<uint32_t> admitted;
            admitted.reserve(std::min(sourceBinCount, desc.MaximumBins));
            uint32_t remainingCapacity = desc.MaximumCommands;
            for (const uint32_t sourceIndex : admissionOrder)
            {
                if (admitted.size() >= desc.MaximumBins)
                    break;
                const uint32_t demand = binDemand[sourceIndex];
                if (demand > desc.MaximumDrawsPerCall || demand > remainingCapacity)
                    continue;
                admitted.push_back(sourceIndex);
                remainingCapacity -= demand;
            }
            // Submission order and fixed offsets stay stable regardless of the
            // admission heuristic used under a constrained command budget.
            std::sort(admitted.begin(), admitted.end());
            bins.reserve(admitted.size());
            uint32_t firstCommand = 0;
            for (const uint32_t sourceIndex : admitted)
            {
                const uint32_t capacity = binDemand[sourceIndex];
                bins.push_back({ uniqueKeys[sourceIndex], firstCommand, capacity, static_cast<uint32_t>(bins.size()) });
                firstCommand += capacity;
            }
            commandCapacity = firstCommand;

            if (!bins.empty())
            {
                const uint32_t lookupCapacity = std::bit_ceil(std::max(static_cast<uint32_t>(bins.size()) * 2u, 2u));
                lookupEntries.resize(lookupCapacity);
                const uint32_t lookupMask = lookupCapacity - 1u;
                for (uint32_t binIndex = 0; binIndex < bins.size(); binIndex++)
                {
                    const GpuDrawBin& bin = bins[binIndex];
                    uint32_t lookupIndex = Hash(bin.Key) & lookupMask;
                    while (lookupEntries[lookupIndex].BinIndex != GpuDrawBinLookupEntry::InvalidBin)
                        lookupIndex = (lookupIndex + 1u) & lookupMask;
                    lookupEntries[lookupIndex] = { static_cast<uint32_t>(bin.Key.Phase),
                                                   static_cast<uint32_t>(bin.Key.Alpha),
                                                   bin.Key.Pipeline,
                                                   bin.Key.GeometryHeap,
                                                   bin.Key.MaterialTemplate,
                                                   binIndex,
                                                   bin.FirstCommand,
                                                   bin.CommandCapacity };
                }
            }
        }

        const bool changed = !(m_Desc == desc) || m_Bins != bins || m_LookupEntries != lookupEntries;
        if (!changed)
            return false;

        const uint64_t version = m_Stats.Version + 1u;
        m_Desc = desc;
        m_Bins = std::move(bins);
        m_LookupEntries = std::move(lookupEntries);
        m_Stats = {};
        m_Stats.SourceBinCount = sourceBinCount;
        m_Stats.ActiveBinCount = static_cast<uint32_t>(m_Bins.size());
        m_Stats.RejectedBinCount = sourceBinCount - m_Stats.ActiveBinCount;
        m_Stats.CommandCapacity = commandCapacity;
        m_Stats.LookupCapacity = static_cast<uint32_t>(m_LookupEntries.size());
        m_Stats.Version = version;
        return true;
    }

    void GpuDrawBinLayout::Reset()
    {
        m_Desc = {};
        m_Bins.clear();
        m_LookupEntries.clear();
        m_Stats = {};
    }

    uint32_t GpuDrawBinLayout::FindBin(const GpuDrawBinKey& key) const
    {
        if (m_LookupEntries.empty())
            return GpuDrawBinLookupEntry::InvalidBin;
        const uint32_t lookupMask = static_cast<uint32_t>(m_LookupEntries.size() - 1u);
        uint32_t lookupIndex = Hash(key) & lookupMask;
        for (uint32_t probe = 0; probe < m_LookupEntries.size(); probe++)
        {
            const GpuDrawBinLookupEntry& entry = m_LookupEntries[lookupIndex];
            if (entry.BinIndex == GpuDrawBinLookupEntry::InvalidBin)
                return GpuDrawBinLookupEntry::InvalidBin;
            if (entry.Phase == static_cast<uint32_t>(key.Phase) && entry.Alpha == static_cast<uint32_t>(key.Alpha) &&
                entry.Pipeline == key.Pipeline && entry.GeometryHeap == key.GeometryHeap && entry.MaterialTemplate == key.MaterialTemplate)
                return entry.BinIndex;
            lookupIndex = (lookupIndex + 1u) & lookupMask;
        }
        return GpuDrawBinLookupEntry::InvalidBin;
    }

    void GpuDrawListBuilder::Reserve(uint32_t candidateCount, uint32_t commandCount)
    {
        m_OrderIndependent.reserve(candidateCount);
        m_StrictTransparent.reserve(candidateCount);
        static_cast<void>(commandCount);
    }

    bool GpuDrawListBuilder::IsStrictTransparent(AlphaMode alpha) { return alpha == AlphaMode::Premultiplied || alpha == AlphaMode::Additive; }

    bool GpuDrawListBuilder::IsOrderIndependent(AlphaMode alpha)
    {
        return alpha == AlphaMode::Opaque || alpha == AlphaMode::Mask || alpha == AlphaMode::WeightedOIT;
    }

    bool GpuDrawListBuilder::SameGeometry(const GpuDrawCandidate& first, const GpuDrawCandidate& second)
    {
        return first.IndexCount == second.IndexCount && first.FirstIndex == second.FirstIndex && first.VertexOffset == second.VertexOffset;
    }

    bool GpuDrawListBuilder::BinLess(const GpuDrawBinKey& first, const GpuDrawBinKey& second)
    {
        if (first.Phase != second.Phase)
            return CompareValue(first.Phase, second.Phase);
        if (first.Alpha != second.Alpha)
            return CompareValue(first.Alpha, second.Alpha);
        if (first.Pipeline != second.Pipeline)
            return first.Pipeline < second.Pipeline;
        if (first.GeometryHeap != second.GeometryHeap)
            return first.GeometryHeap < second.GeometryHeap;
        return first.MaterialTemplate < second.MaterialTemplate;
    }

    void GpuDrawListBuilder::Build(const GpuDrawCandidate* candidates, uint32_t candidateCount, GpuDrawList& output)
    {
        output.Clear();
        m_OrderIndependent.clear();
        m_StrictTransparent.clear();
        m_OrderIndependent.reserve(std::max<size_t>(m_OrderIndependent.capacity(), candidateCount));
        m_StrictTransparent.reserve(std::max<size_t>(m_StrictTransparent.capacity(), candidateCount));

        for (uint32_t index = 0; candidates != nullptr && index < candidateCount; index++)
        {
            if (candidates[index].IndexCount == 0)
                continue;
            SortEntry entry{ candidates[index], index };
            if (IsStrictTransparent(entry.Candidate.Bin.Alpha))
                m_StrictTransparent.push_back(entry);
            else if (IsOrderIndependent(entry.Candidate.Bin.Alpha))
                m_OrderIndependent.push_back(entry);
        }

        std::sort(m_OrderIndependent.begin(), m_OrderIndependent.end(), [](const SortEntry& first, const SortEntry& second) {
            if (first.Candidate.RenderLayer != second.Candidate.RenderLayer)
                return first.Candidate.RenderLayer < second.Candidate.RenderLayer;
            if (!(first.Candidate.Bin == second.Candidate.Bin))
                return BinLess(first.Candidate.Bin, second.Candidate.Bin);
            if (!SameGeometry(first.Candidate, second.Candidate))
            {
                if (first.Candidate.FirstIndex != second.Candidate.FirstIndex)
                    return first.Candidate.FirstIndex < second.Candidate.FirstIndex;
                if (first.Candidate.IndexCount != second.Candidate.IndexCount)
                    return first.Candidate.IndexCount < second.Candidate.IndexCount;
                return first.Candidate.VertexOffset < second.Candidate.VertexOffset;
            }
            if (first.Candidate.ViewDepth < second.Candidate.ViewDepth)
                return true;
            if (second.Candidate.ViewDepth < first.Candidate.ViewDepth)
                return false;
            return first.StableIndex < second.StableIndex;
        });

        std::sort(m_StrictTransparent.begin(), m_StrictTransparent.end(), [](const SortEntry& first, const SortEntry& second) {
            if (first.Candidate.RenderLayer != second.Candidate.RenderLayer)
                return first.Candidate.RenderLayer < second.Candidate.RenderLayer;
            if (first.Candidate.ViewDepth > second.Candidate.ViewDepth)
                return true;
            if (second.Candidate.ViewDepth > first.Candidate.ViewDepth)
                return false;
            return first.StableIndex < second.StableIndex;
        });

        output.Instances.reserve(candidateCount);
        output.Commands.reserve(candidateCount);
        output.Runs.reserve(candidateCount);

        auto beginRun = [&](const GpuDrawBinKey& key) {
            if (output.Runs.empty() || !(output.Runs.back().Bin == key))
                output.Runs.push_back({ key, static_cast<uint32_t>(output.Commands.size()), 0u });
        };
        auto appendCommand = [&](const GpuDrawCandidate& candidate) {
            const uint32_t firstInstance = static_cast<uint32_t>(output.Instances.size());
            output.Instances.push_back({ candidate.InstanceID, candidate.MaterialIndex });
            output.Commands.push_back({ candidate.IndexCount, 1u, candidate.FirstIndex, candidate.VertexOffset, firstInstance });
            output.Runs.back().CommandCount++;
        };

        for (uint32_t index = 0; index < m_OrderIndependent.size(); index++)
        {
            const SortEntry& entry = m_OrderIndependent[index];
            const GpuDrawCandidate& candidate = entry.Candidate;
            beginRun(candidate.Bin);
            if (index != 0 && output.Runs.back().CommandCount != 0)
            {
                DrawIndexedIndirectCommand& command = output.Commands.back();
                const bool sameRun = output.Runs.back().FirstCommand + output.Runs.back().CommandCount == output.Commands.size();
                const GpuDrawCandidate& previous = m_OrderIndependent[index - 1u].Candidate;
                if (sameRun && previous.Bin == candidate.Bin && SameGeometry(previous, candidate))
                {
                    output.Instances.push_back({ candidate.InstanceID, candidate.MaterialIndex });
                    command.InstanceCount++;
                    continue;
                }
            }
            appendCommand(candidate);
        }

        for (const SortEntry& entry : m_StrictTransparent)
        {
            beginRun(entry.Candidate.Bin);
            appendCommand(entry.Candidate);
            output.StrictTransparentCommandCount++;
        }
    }

    void GpuDrawBuffers::Upload(const GpuDrawList& list)
    {
        m_Stats.UploadedBytes = 0;
        EnsureCapacity(static_cast<uint32_t>(list.Instances.size()), static_cast<uint32_t>(list.Commands.size()),
                       static_cast<uint32_t>(list.Runs.size()));
        if (!HasGpuBuffers())
            return;

        if (!list.Instances.empty())
        {
            const uint32_t size = static_cast<uint32_t>(list.Instances.size() * sizeof(GpuVisibleDrawInstance));
            m_InstanceIDs->WriteData(0, size, list.Instances.data(), BWT_NORMAL);
            m_Stats.UploadedBytes += size;
        }
        if (!list.Commands.empty())
        {
            const uint32_t size = static_cast<uint32_t>(list.Commands.size() * sizeof(DrawIndexedIndirectCommand));
            m_Commands->WriteData(0, size, list.Commands.data(), BWT_NORMAL);
            m_Stats.UploadedBytes += size;
        }
        if (!list.Runs.empty())
        {
            m_RunCounts.clear();
            m_RunCounts.reserve(list.Runs.size());
            for (const GpuDrawRun& run : list.Runs)
                m_RunCounts.push_back(run.CommandCount);
            const uint32_t size = static_cast<uint32_t>(m_RunCounts.size() * sizeof(uint32_t));
            m_Counts->WriteData(0, size, m_RunCounts.data(), BWT_NORMAL);
            m_Stats.UploadedBytes += size;
        }
    }

    void GpuDrawBuffers::Reset()
    {
        m_InstanceIDs = nullptr;
        m_Commands = nullptr;
        m_Counts = nullptr;
        Vector<uint32_t>().swap(m_RunCounts);
        m_Stats = {};
    }

    bool GpuDrawBuffers::CanCreateGpuBuffers() const
    {
        return m_EnableGpuBuffers && RenderAPI::TryGet() != nullptr && RenderAPI::TryGet()->GetCapabilities().HasCapability(CW_LOAD_STORE);
    }

    void GpuDrawBuffers::EnsureCapacity(uint32_t instanceCount, uint32_t commandCount, uint32_t runCount)
    {
        if (!CanCreateGpuBuffers())
            return;
        if (instanceCount > m_Stats.InstanceCapacity || (instanceCount != 0 && m_InstanceIDs == nullptr))
        {
            m_Stats.InstanceCapacity = NextCapacity(instanceCount, 1024u);
            m_InstanceIDs = CreateBuffer(m_Stats.InstanceCapacity, sizeof(GpuVisibleDrawInstance), GpuBufferType::Structured);
        }
        if (commandCount > m_Stats.CommandCapacity || (commandCount != 0 && m_Commands == nullptr))
        {
            m_Stats.CommandCapacity = NextCapacity(commandCount, 1024u);
            m_Commands = CreateBuffer(m_Stats.CommandCapacity, sizeof(DrawIndexedIndirectCommand), GpuBufferType::IndirectDraw);
        }
        if (runCount > m_Stats.RunCapacity || (runCount != 0 && m_Counts == nullptr))
        {
            m_Stats.RunCapacity = NextCapacity(runCount, 64u);
            m_Counts = CreateBuffer(m_Stats.RunCapacity, sizeof(uint32_t), GpuBufferType::IndirectDraw);
        }
    }
} // namespace Crowny

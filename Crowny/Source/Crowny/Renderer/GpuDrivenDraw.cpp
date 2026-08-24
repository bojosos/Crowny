#include "cwpch.h"

#include "Crowny/Renderer/GpuDrivenDraw.h"

#include "Crowny/RenderAPI/RenderCapabilities.h"

#include <bit>

namespace Crowny
{
    namespace
    {
        template <typename T> bool CompareValue(const T& first, const T& second)
        {
            return first < second;
        }

        uint32_t NextCapacity(uint32_t required, uint32_t minimum)
        {
            return std::max(std::bit_ceil(std::max(required, 1u)), minimum);
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

    void GpuDrawListBuilder::Reserve(uint32_t candidateCount, uint32_t commandCount)
    {
        m_OrderIndependent.reserve(candidateCount);
        m_StrictTransparent.reserve(candidateCount);
        static_cast<void>(commandCount);
    }

    bool GpuDrawListBuilder::IsStrictTransparent(AlphaMode alpha)
    {
        return alpha == AlphaMode::Premultiplied || alpha == AlphaMode::Additive;
    }

    bool GpuDrawListBuilder::IsOrderIndependent(AlphaMode alpha)
    {
        return alpha == AlphaMode::Opaque || alpha == AlphaMode::Mask || alpha == AlphaMode::WeightedOIT;
    }

    bool GpuDrawListBuilder::SameGeometry(const GpuDrawCandidate& first, const GpuDrawCandidate& second)
    {
        return first.IndexCount == second.IndexCount && first.FirstIndex == second.FirstIndex &&
               first.VertexOffset == second.VertexOffset;
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

        std::stable_sort(m_OrderIndependent.begin(), m_OrderIndependent.end(), [](const SortEntry& first, const SortEntry& second) {
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
            if (first.Candidate.ViewDepth != second.Candidate.ViewDepth)
                return first.Candidate.ViewDepth < second.Candidate.ViewDepth;
            return first.StableIndex < second.StableIndex;
        });

        std::stable_sort(m_StrictTransparent.begin(), m_StrictTransparent.end(), [](const SortEntry& first, const SortEntry& second) {
            if (first.Candidate.RenderLayer != second.Candidate.RenderLayer)
                return first.Candidate.RenderLayer < second.Candidate.RenderLayer;
            if (first.Candidate.ViewDepth != second.Candidate.ViewDepth)
                return first.Candidate.ViewDepth > second.Candidate.ViewDepth;
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
            Vector<uint32_t> counts;
            counts.reserve(list.Runs.size());
            for (const GpuDrawRun& run : list.Runs)
                counts.push_back(run.CommandCount);
            const uint32_t size = static_cast<uint32_t>(counts.size() * sizeof(uint32_t));
            m_Counts->WriteData(0, size, counts.data(), BWT_NORMAL);
            m_Stats.UploadedBytes += size;
        }
    }

    void GpuDrawBuffers::Reset()
    {
        m_InstanceIDs = nullptr;
        m_Commands = nullptr;
        m_Counts = nullptr;
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

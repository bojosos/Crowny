#include "cwpch.h"

#include "Crowny/Renderer/GpuRadixSort.h"

#include <bit>

namespace Crowny
{
    GpuRadixSortPlan GpuRadixSort::BuildPlan(uint32_t elementCount, uint32_t groupSize)
    {
        GpuRadixSortPlan plan;
        plan.ElementCount = elementCount;
        plan.GroupSize = std::clamp(std::bit_floor(std::max(groupSize, 1u)), 32u, 256u);
        plan.GroupCount = (elementCount + plan.GroupSize - 1u) / plan.GroupSize;
        const uint64_t histogramEntries = static_cast<uint64_t>(plan.GroupCount) * plan.Radix;
        plan.HistogramBytes = histogramEntries * sizeof(uint32_t);
        plan.GroupOffsetBytes = histogramEntries * sizeof(uint32_t);
        plan.ScratchKeyBytes = static_cast<uint64_t>(elementCount) * sizeof(glm::uvec4);
        plan.ScratchValueBytes = static_cast<uint64_t>(elementCount) * sizeof(uint32_t);
        return plan;
    }

    uint32_t GpuRadixSort::Digit(uint32_t keyHigh, uint32_t keyLow, uint32_t passIndex)
    {
        passIndex = std::min(passIndex, 7u);
        return passIndex < 4u ? (keyLow >> (passIndex * 8u)) & 0xffu
                              : (keyHigh >> ((passIndex - 4u) * 8u)) & 0xffu;
    }
} // namespace Crowny

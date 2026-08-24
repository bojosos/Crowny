#pragma once

#include "Crowny/Common/Types.h"

namespace Crowny
{
    struct GpuRadixSortPlan
    {
        uint32_t ElementCount = 0;
        uint32_t GroupSize = 256;
        uint32_t GroupCount = 0;
        uint32_t RadixBits = 8;
        uint32_t Radix = 256;
        uint32_t PassCount = 8;
        uint64_t HistogramBytes = 0;
        uint64_t GroupOffsetBytes = 0;
        uint64_t ScratchKeyBytes = 0;
        uint64_t ScratchValueBytes = 0;

        uint64_t GetScratchBytes() const
        {
            return HistogramBytes + GroupOffsetBytes + ScratchKeyBytes + ScratchValueBytes;
        }
    };

    class GpuRadixSort
    {
    public:
        static GpuRadixSortPlan BuildPlan(uint32_t elementCount, uint32_t groupSize = 256);
        static uint32_t Digit(uint32_t keyHigh, uint32_t keyLow, uint32_t passIndex);
    };
} // namespace Crowny

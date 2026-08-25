#pragma once

#include <cstddef>
#include <cstdint>

namespace Crowny::Memory
{
    struct ThreadAllocationSnapshot
    {
        uint64_t AllocationCount = 0;
        uint64_t RequestedBytes = 0;
    };

    /** Returns monotonic allocation totals for the calling thread. This function never allocates. */
    ThreadAllocationSnapshot GetThreadAllocationSnapshot() noexcept;

    /** Calculates the allocations made between two snapshots from the same thread. */
    ThreadAllocationSnapshot GetThreadAllocationDelta(const ThreadAllocationSnapshot& previous, const ThreadAllocationSnapshot& current) noexcept;

    namespace Detail
    {
        void RecordThreadAllocation(size_t size) noexcept;
    }
} // namespace Crowny::Memory

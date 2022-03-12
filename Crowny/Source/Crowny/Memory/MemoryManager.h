#pragma once

#include <spdlog/fmt/ostr.h>

namespace Crowny
{

    struct MemoryStats
    {
        uint64_t TotalAllocated = 0;
        uint64_t TotalFreed = 0;
        uint64_t CurrentAllcoated = 0;
        uint64_t Allocations = 0;

        MemoryStats() : TotalAllocated(0), TotalFreed(0), CurrentAllcoated(0), Allocations(0) {}

        template <typename OStream> friend OStream& operator<<(OStream& os, const MemoryStats& ms)
        {
            return os << "Allocated: " << ms.TotalAllocated << " bytes\nFreed: " << ms.TotalFreed
                      << " bytes\nCurrently Allocated: " << ms.CurrentAllcoated
                      << " bytes\nTotal Allocations: " << ms.Allocations << " bytes";
        }
    };

    struct SystemMemoryInfo
    {
        uint64_t AvaliablePhysicalMemory = 0;
        uint64_t TotalPhysicalMemory = 0;

        uint64_t AvailableVirtualMemory = 0;
        uint64_t TotalVirtualMemory = 0;

        template <typename OStream> friend OStream& operator<<(OStream& os, const SystemMemoryInfo& smi)
        {
            return os << "Avaliable Physical Memory: " << smi.AvaliablePhysicalMemory << "/" << smi.TotalPhysicalMemory
                      << " bytes\nAvaliable Virtual Memory: " << smi.AvailableVirtualMemory << "/"
                      << smi.TotalVirtualMemory << " bytes";
        }
    };

    class MemoryManager
    {
    public:
        MemoryManager() = default;
        static void Init();
        static void Shutdown();

        static MemoryManager& Get()
        {
            static MemoryManager s_Instance;
            return s_Instance;
        }

        MemoryStats GetMemoryStats() const { return m_MemoryStats; }
        SystemMemoryInfo GetSystemInfo();

        MemoryStats m_MemoryStats;
        SystemMemoryInfo m_SystemInfo;
    };
} // namespace Crowny

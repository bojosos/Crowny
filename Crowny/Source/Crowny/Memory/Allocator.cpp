#include "cwpch.h"

#include "Crowny/Memory/Allocator.h"
#include "Crowny/Memory/MemoryManager.h"

namespace Crowny
{

    static AllocationStats s_GlobalStats;
    static bool s_InInit = false;

    void Allocator::Init()
    {
        if (s_Data != nullptr)
            return;
        s_InInit = true;
        AllocatorData* data = (AllocatorData*)Allocator::AllocateRaw(sizeof(AllocatorData));
        new (data) AllocatorData();
        s_Data = data;
        s_InInit = false;
    }

    void* Allocator::AllocateRaw(size_t size) { return malloc(size); }

    void* Allocator::Allocate(size_t size)
    {
        if (s_InInit)
            return AllocateRaw(size);
        if (!s_Data)
            Init();
        void* memory = malloc(size);
        {
            Lock lock(s_Data->m_Mutex);
            Allocation& alloc = s_Data->m_AllocationMap[memory];
            alloc.Memory = memory;
            alloc.Size = size;
            s_GlobalStats.TotalAllocated += size;
        }
        return memory;
    }

    void* Allocator::Allocate(size_t size, const char* desc)
    {
        if (!s_Data)
            Init();

        void* memory = malloc(size);

        {
            std::scoped_lock<std::mutex> lock(s_Data->m_Mutex);
            Allocation& alloc = s_Data->m_AllocationMap[memory];
            alloc.Memory = memory;
            alloc.Size = size;
            alloc.Category = desc;

            s_GlobalStats.TotalAllocated += size;
            if (desc)
                s_Data->m_AllocationStatsMap[desc].TotalAllocated += size;
        }

        return memory;
    }

    void* Allocator::Allocate(size_t size, const char* file, int line)
    {
        if (!s_Data)
            Init();

        void* memory = malloc(size);

        {
            std::scoped_lock<std::mutex> lock(s_Data->m_Mutex);
            Allocation& alloc = s_Data->m_AllocationMap[memory];
            alloc.Memory = memory;
            alloc.Size = size;
            alloc.Category = file;

            s_GlobalStats.TotalAllocated += size;
            s_Data->m_AllocationStatsMap[file].TotalAllocated += size;
        }

        return memory;
    }

    void Allocator::Free(void* memory)
    {
        if (memory == nullptr)
            return;

        bool found = false;
        {
            std::scoped_lock<std::mutex> lock(s_Data->m_Mutex);
            auto iterFind = s_Data->m_AllocationMap.find(memory);
            found = iterFind != s_Data->m_AllocationMap.end();
            if (found)
            {
                const Allocation& alloc = iterFind->second;
                s_GlobalStats.TotalFreed += alloc.Size;
                if (alloc.Category)
                    s_Data->m_AllocationStatsMap[alloc.Category].TotalFreed += alloc.Size;

                s_Data->m_AllocationMap.erase(memory);
            }
        }
        if (!found)
            CW_ENGINE_WARN("Memory: Memory block {0} not present in alloc map", memory);

        free(memory);
    }

    namespace Memory
    {
        const Crowny::AllocationStats& GetAllocationStats() { return s_GlobalStats; }
    } // namespace Memory

} // namespace Crowny
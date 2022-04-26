#pragma once

namespace Crowny
{

    struct AllocationStats
    {
        size_t TotalAllocated = 0;
        size_t TotalFreed = 0;
    };

    struct Allocation
    {
        void* Memory = 0;
        size_t Size = 0;
        const char* Category;
    };

    template <class T> struct Mallocator
    {
        typedef T value_type;

        Mallocator() = default;
        template <class U> constexpr Mallocator(const Mallocator<U>&) noexcept {}

        T* allocate(std::size_t n)
        {
#undef max
            if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
                throw std::bad_array_new_length();

            if (auto p = static_cast<T*>(std::malloc(n * sizeof(T))))
            {
                return p;
            }

            throw std::bad_alloc();
        }

        void deallocate(T* p, std::size_t n) noexcept { std::free(p); }
    };

    struct AllocatorData
    {
        using MapAlloc = Mallocator<std::pair<const void* const, Allocation>>;
        using StatsMapAlloc = Mallocator<std::pair<const char* const, AllocationStats>>;

        using AllocationStatsMap = std::map<const char*, AllocationStats, std::less<const char*>, StatsMapAlloc>;
        std::map<const void*, Allocation, std::less<const void*>, MapAlloc> m_AllocationMap;
        AllocationStatsMap m_AllocationStatsMap;
        Mutex m_Mutex, m_StatsMutex;
    };

    namespace Memory
    {
        const AllocationStats& GetAllocationStats();
    }

    class Allocator
    {
    public:
        static void Init();
        static void* AllocateRaw(size_t size);
        static void* Allocate(size_t size);
        static void* Allocate(size_t size, const char* desc);
        static void* Allocate(size_t size, const char* file, int line);

        static void Free(void* block);
        static const AllocatorData::AllocationStatsMap& GetAllocationStats() { return s_Data->m_AllocationStatsMap; }

    private:
        inline static AllocatorData* s_Data = nullptr;
    };

} // namespace Crowny
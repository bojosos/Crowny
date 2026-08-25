#include "cwpch.h"

#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Memory/Allocator.h"
#include "Crowny/Memory/Memory.h"

#include <cstdlib>
#include <new>

#ifdef CW_PLATFORM_WIN32
#include <malloc.h>
#endif

namespace Crowny::Memory
{
    namespace
    {
        thread_local ThreadAllocationSnapshot s_ThreadAllocations;
    }

    ThreadAllocationSnapshot GetThreadAllocationSnapshot() noexcept { return s_ThreadAllocations; }

    ThreadAllocationSnapshot GetThreadAllocationDelta(const ThreadAllocationSnapshot& previous, const ThreadAllocationSnapshot& current) noexcept
    {
        return { current.AllocationCount - previous.AllocationCount, current.RequestedBytes - previous.RequestedBytes };
    }

    namespace Detail
    {
        void RecordThreadAllocation(size_t size) noexcept
        {
            s_ThreadAllocations.AllocationCount++;
            s_ThreadAllocations.RequestedBytes += size;
        }
    } // namespace Detail
} // namespace Crowny::Memory

namespace
{
    void* AllocateUnaligned(size_t requestedSize)
    {
        const size_t size = requestedSize == 0 ? 1 : requestedSize;
        while (true)
        {
#if CW_TRACK_MEMORY
            void* memory = Crowny::Allocator::Allocate(size);
#else
            void* memory = std::malloc(size);
#endif
            if (memory != nullptr)
            {
                Crowny::Memory::Detail::RecordThreadAllocation(requestedSize);
                return memory;
            }

            const std::new_handler handler = std::get_new_handler();
            if (handler == nullptr)
                throw std::bad_alloc();
            handler();
        }
    }

    void* AllocateAligned(size_t requestedSize, size_t alignment)
    {
        const size_t size = requestedSize == 0 ? 1 : requestedSize;
        while (true)
        {
#ifdef CW_PLATFORM_WIN32
            void* memory = _aligned_malloc(size, alignment);
#else
            void* memory = nullptr;
            if (posix_memalign(&memory, alignment, size) != 0)
                memory = nullptr;
#endif
            if (memory != nullptr)
            {
                Crowny::Memory::Detail::RecordThreadAllocation(requestedSize);
                return memory;
            }

            const std::new_handler handler = std::get_new_handler();
            if (handler == nullptr)
                throw std::bad_alloc();
            handler();
        }
    }

    void FreeUnaligned(void* memory) noexcept
    {
#if CW_TRACK_MEMORY
        Crowny::Allocator::Free(memory);
#else
        std::free(memory);
#endif
    }

    void FreeAligned(void* memory) noexcept
    {
#ifdef CW_PLATFORM_WIN32
        _aligned_free(memory);
#else
        std::free(memory);
#endif
    }
} // namespace

void* operator new(size_t size) { return AllocateUnaligned(size); }
void* operator new[](size_t size) { return AllocateUnaligned(size); }

void* operator new(size_t size, const std::nothrow_t&) noexcept
{
    try
    {
        return AllocateUnaligned(size);
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept
{
    try
    {
        return AllocateUnaligned(size);
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new(size_t size, std::align_val_t alignment) { return AllocateAligned(size, static_cast<size_t>(alignment)); }
void* operator new[](size_t size, std::align_val_t alignment) { return AllocateAligned(size, static_cast<size_t>(alignment)); }

void* operator new(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    try
    {
        return AllocateAligned(size, static_cast<size_t>(alignment));
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[](size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    try
    {
        return AllocateAligned(size, static_cast<size_t>(alignment));
    }
    catch (...)
    {
        return nullptr;
    }
}

void operator delete(void* memory) noexcept { FreeUnaligned(memory); }
void operator delete[](void* memory) noexcept { FreeUnaligned(memory); }
void operator delete(void* memory, size_t) noexcept { FreeUnaligned(memory); }
void operator delete[](void* memory, size_t) noexcept { FreeUnaligned(memory); }
void operator delete(void* memory, const std::nothrow_t&) noexcept { FreeUnaligned(memory); }
void operator delete[](void* memory, const std::nothrow_t&) noexcept { FreeUnaligned(memory); }

void operator delete(void* memory, std::align_val_t) noexcept { FreeAligned(memory); }
void operator delete[](void* memory, std::align_val_t) noexcept { FreeAligned(memory); }
void operator delete(void* memory, size_t, std::align_val_t) noexcept { FreeAligned(memory); }
void operator delete[](void* memory, size_t, std::align_val_t) noexcept { FreeAligned(memory); }
void operator delete(void* memory, std::align_val_t, const std::nothrow_t&) noexcept { FreeAligned(memory); }
void operator delete[](void* memory, std::align_val_t, const std::nothrow_t&) noexcept { FreeAligned(memory); }

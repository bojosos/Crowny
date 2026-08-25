#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Memory/FrameVector.h"
#include "Crowny/Threading/CommandQueue.h"
#include "cwpch.h"

#include <atomic>
#include <new>
#include <thread>

using namespace Crowny;

namespace
{
    template <typename T> struct AllocationCounter
    {
        static inline std::atomic<size_t> Allocations{ 0 };
    };

    template <typename T> class CountingAllocator
    {
    public:
        using value_type = T;

        CountingAllocator() = default;
        template <typename U> CountingAllocator(const CountingAllocator<U>&) noexcept {}

        T* allocate(size_t count)
        {
            AllocationCounter<T>::Allocations.fetch_add(1, std::memory_order_relaxed);
            return std::allocator<T>{}.allocate(count);
        }

        void deallocate(T* data, size_t count) noexcept { std::allocator<T>{}.deallocate(data, count); }

        template <typename U> bool operator==(const CountingAllocator<U>&) const noexcept { return true; }
        template <typename U> bool operator!=(const CountingAllocator<U>&) const noexcept { return false; }
    };

    struct FramePayload
    {
        std::vector<uint32_t, CountingAllocator<uint32_t>> NestedValues;
    };
} // namespace

TEST_CASE("FrameVector retains nested storage across frame resets", "[Memory][Frame]")
{
    constexpr size_t FrameCount = 120;
    constexpr size_t ObjectCount = 128;
    constexpr size_t ValuesPerObject = 8;

    AllocationCounter<uint32_t>::Allocations.store(0, std::memory_order_relaxed);
    for (size_t frameIndex = 0; frameIndex < FrameCount; frameIndex++)
    {
        Vector<FramePayload> freshFrame;
        freshFrame.reserve(ObjectCount);
        for (size_t objectIndex = 0; objectIndex < ObjectCount; objectIndex++)
        {
            FramePayload& payload = freshFrame.emplace_back();
            payload.NestedValues.assign(ValuesPerObject, static_cast<uint32_t>(objectIndex));
        }
    }
    const size_t freshAllocations = AllocationCounter<uint32_t>::Allocations.load(std::memory_order_relaxed);

    AllocationCounter<uint32_t>::Allocations.store(0, std::memory_order_relaxed);
    FrameVector<FramePayload> reusedFrame;
    reusedFrame.Reserve(ObjectCount);
    for (size_t frameIndex = 0; frameIndex < FrameCount; frameIndex++)
    {
        reusedFrame.Reset();
        for (size_t objectIndex = 0; objectIndex < ObjectCount; objectIndex++)
        {
            FramePayload& payload = reusedFrame.Acquire();
            payload.NestedValues.assign(ValuesPerObject, static_cast<uint32_t>(objectIndex));
        }
    }
    const size_t reusedAllocations = AllocationCounter<uint32_t>::Allocations.load(std::memory_order_relaxed);

    CHECK(freshAllocations == FrameCount * ObjectCount);
    CHECK(reusedAllocations == ObjectCount);
    CHECK(reusedFrame.RetainedSize() == ObjectCount);
    CHECK(reusedFrame.Size() == ObjectCount);
}

TEST_CASE("CommandQueue reuses both frame buffers", "[Memory][Frame][Threading]")
{
    constexpr size_t CommandsPerFrame = 8;
    CommandQueue queue(CommandsPerFrame);
    uint32_t executions = 0;

    for (uint32_t frameIndex = 0; frameIndex < 120; frameIndex++)
    {
        for (size_t commandIndex = 0; commandIndex < CommandsPerFrame; commandIndex++)
            queue.Enqueue([&executions]() { executions++; });

        queue.Swap();
        queue.DrainAndExecute();
    }

    const CommandQueue::Metrics metrics = queue.GetMetrics();
    CHECK(executions == 120 * CommandsPerFrame);
    CHECK(metrics.WriteCapacity == CommandsPerFrame);
    CHECK(metrics.ReadCapacity == CommandsPerFrame);
    CHECK(metrics.WriteSize == 0);
    CHECK(metrics.ReadSize == 0);
}

TEST_CASE("Thread allocation snapshots cover standard allocation families", "[Memory][Frame]")
{
    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();

    void* scalar = ::operator new(16);
    void* array = ::operator new[](32, std::nothrow);
    void* alignedScalar = ::operator new(64, std::align_val_t(64));
    void* alignedArray = ::operator new[](128, std::align_val_t(64), std::nothrow);

    ::operator delete(scalar, size_t{ 16 });
    ::operator delete[](array, std::nothrow);
    ::operator delete(alignedScalar, size_t{ 64 }, std::align_val_t(64));
    ::operator delete[](alignedArray, std::align_val_t(64), std::nothrow);

    const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();
    const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, after);
    CHECK(delta.AllocationCount == 4);
    CHECK(delta.RequestedBytes == 240);
}

TEST_CASE("Thread allocation snapshots isolate worker allocations", "[Memory][Frame][Threading]")
{
    std::atomic<bool> workerReady{ false };
    std::atomic<bool> startWorker{ false };
    std::atomic<bool> workerDone{ false };
    std::atomic<uint64_t> workerAllocationCount{ 0 };

    std::thread worker([&]() {
        const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
        workerReady.store(true, std::memory_order_release);
        while (!startWorker.load(std::memory_order_acquire))
            std::this_thread::yield();

        void* memory = ::operator new(96);
        ::operator delete(memory);
        const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();
        workerAllocationCount.store(Memory::GetThreadAllocationDelta(before, after).AllocationCount, std::memory_order_release);
        workerDone.store(true, std::memory_order_release);
    });

    while (!workerReady.load(std::memory_order_acquire))
        std::this_thread::yield();
    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    startWorker.store(true, std::memory_order_release);
    while (!workerDone.load(std::memory_order_acquire))
        std::this_thread::yield();
    const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();
    worker.join();

    CHECK(Memory::GetThreadAllocationDelta(before, after).AllocationCount == 0);
    CHECK(workerAllocationCount.load(std::memory_order_acquire) == 1);
}

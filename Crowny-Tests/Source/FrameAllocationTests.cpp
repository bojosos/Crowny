#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/FrameVector.h"
#include "Crowny/Threading/CommandQueue.h"
#include "cwpch.h"

#include <atomic>

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

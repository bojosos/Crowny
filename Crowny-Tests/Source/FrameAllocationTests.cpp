#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Memory/FrameVector.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Physics/PhysicsCollision.h"
#include "Crowny/Renderer/RenderPipeline.h"
#include "Crowny/Scene/SceneRenderer.h"
#include "Crowny/Threading/CommandQueue.h"
#include "cwpch.h"

#include <atomic>
#include <glm/gtc/matrix_transform.hpp>
#include <new>
#include <thread>

using namespace Crowny;

namespace
{
    class RecordingHistoryReleaseSink final : public RenderHistoryReleaseSink
    {
    public:
        void QueueHistoryRelease(uint64_t historyNamespace) override { ReleasedNamespaces.push_back(historyNamespace); }

        Vector<uint64_t> ReleasedNamespaces;
    };

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

TEST_CASE("Scene snapshot extraction preserves the frame-context number", "[Memory][Frame][Renderer][SceneSync]")
{
    const Ref<Scene> scene = CreateRef<Scene>(false);
    SceneRenderer renderer(scene, nullptr);
    RenderSnapshot snapshot;
    RenderPipelineSettings settings;
    settings.SharpeningStrength = 0.75f;
    renderer.SetRenderPipelineSettings(settings);

    snapshot.FrameNumber = 73;
    renderer.ExtractSnapshot(snapshot, false);
    CHECK(snapshot.FrameNumber == 73);
    CHECK(snapshot.PipelineSettings.SharpeningStrength == 0.75f);
    CHECK(renderer.GetRenderPipelineSettings().SharpeningStrength == 0.75f);

    Camera camera;
    snapshot.FrameNumber = 74;
    renderer.ExtractSnapshot(snapshot, camera, glm::mat4(1.0f), false);
    CHECK(snapshot.FrameNumber == 74);
}

TEST_CASE("RenderWorld transform settling reuses its change queues", "[Memory][Frame][Renderer][MotionVectors]")
{
    RenderWorld world(1);
    const RenderInstanceHandle handle = world.CreateInstance({});
    Vector<RenderWorldChange> changes;
    world.DrainChanges(changes);

    for (uint32_t frame = 1; frame <= 4; frame++)
    {
        REQUIRE(world.UpdateTransform(handle, glm::translate(glm::mat4(1.0f), glm::vec3(static_cast<float>(frame), 0.0f, 0.0f)),
                                      glm::vec4(static_cast<float>(frame), 0.0f, 0.0f, 1.0f)));
        world.DrainChanges(changes);
    }

    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    for (uint32_t frame = 5; frame < 125; frame++)
    {
        world.UpdateTransform(handle, glm::translate(glm::mat4(1.0f), glm::vec3(static_cast<float>(frame), 0.0f, 0.0f)),
                              glm::vec4(static_cast<float>(frame), 0.0f, 0.0f, 1.0f));
        world.DrainChanges(changes);
    }
    const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();
    const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, after);

    CHECK(delta.AllocationCount == 0);
    CHECK(delta.RequestedBytes == 0);
}

TEST_CASE("Changing scenes retires the old camera history namespace", "[Memory][Frame][Renderer][SceneSync]")
{
    const Ref<Scene> firstScene = CreateRef<Scene>(false);
    const Ref<Scene> secondScene = CreateRef<Scene>(false);
    SceneRenderer renderer(firstScene, nullptr);
    RenderSnapshot snapshot;
    Camera camera;

    renderer.ExtractSnapshot(snapshot, camera, glm::mat4(1.0f), false);
    const uint64_t firstNamespace = snapshot.HistoryNamespace;
    REQUIRE(firstNamespace != 0);
    CHECK(snapshot.ReleasedHistoryNamespaces.Empty());

    renderer.SetScene(secondScene);
    renderer.ExtractSnapshot(snapshot, camera, glm::mat4(1.0f), false);
    REQUIRE(snapshot.ReleasedHistoryNamespaces.Size() == 1);
    CHECK(snapshot.ReleasedHistoryNamespaces[0] == firstNamespace);
    CHECK(snapshot.HistoryNamespace != firstNamespace);
}

TEST_CASE("Renderer views keep independent camera history namespaces", "[Memory][Frame][Renderer][SceneSync]")
{
    const Ref<Scene> scene = CreateRef<Scene>(false);
    SceneRenderer firstRenderer(scene, nullptr);
    SceneRenderer secondRenderer(scene, nullptr);
    RenderSnapshot firstSnapshot;
    RenderSnapshot secondSnapshot;
    Camera camera;

    firstRenderer.ExtractSnapshot(firstSnapshot, camera, glm::mat4(1.0f), false);
    secondRenderer.ExtractSnapshot(secondSnapshot, camera, glm::mat4(1.0f), false);

    REQUIRE(firstSnapshot.HistoryNamespace != 0);
    REQUIRE(secondSnapshot.HistoryNamespace != 0);
    REQUIRE(firstSnapshot.HistoryOwnerId != 0);
    REQUIRE(secondSnapshot.HistoryOwnerId != 0);
    CHECK(firstSnapshot.HistoryOwnerId != secondSnapshot.HistoryOwnerId);
    CHECK(firstSnapshot.HistoryNamespace != secondSnapshot.HistoryNamespace);
}

TEST_CASE("Inactive camera histories retire after the retention window", "[Memory][Frame][Renderer][SceneSync]")
{
    const Ref<Scene> scene = CreateRef<Scene>(false);
    SceneRenderer renderer(scene, nullptr);
    RenderSnapshot snapshot;
    std::array<Camera, 122> cameras;

    snapshot.FrameNumber = 1;
    renderer.ExtractSnapshot(snapshot, cameras[0], glm::mat4(1.0f), false);
    const uint64_t firstNamespace = snapshot.HistoryNamespace;
    for (size_t index = 1; index < cameras.size(); index++)
    {
        snapshot.FrameNumber = index + 1u;
        renderer.ExtractSnapshot(snapshot, cameras[index], glm::mat4(1.0f), false);
    }

    REQUIRE(snapshot.ReleasedHistoryNamespaces.Size() == 1);
    CHECK(snapshot.ReleasedHistoryNamespaces[0] == firstNamespace);
}

TEST_CASE("Active cameras do not age each other within one frame", "[Memory][Frame][Renderer][SceneSync]")
{
    const Ref<Scene> scene = CreateRef<Scene>(false);
    SceneRenderer renderer(scene, nullptr);
    RenderSnapshot snapshot;
    std::array<Camera, 121> cameras;

    for (uint64_t frame = 1; frame <= 3; frame++)
    {
        for (Camera& camera : cameras)
        {
            snapshot.FrameNumber = frame;
            renderer.ExtractSnapshot(snapshot, camera, glm::mat4(1.0f), false);
            if (frame != 1)
                CHECK_FALSE(snapshot.CameraCut);
            CHECK(snapshot.ReleasedHistoryNamespaces.Empty());
        }
    }
}

TEST_CASE("Value-returning snapshots assign implicit frame numbers", "[Memory][Frame][Renderer][SceneSync]")
{
    const Ref<Scene> scene = CreateRef<Scene>(false);
    SceneRenderer renderer(scene, nullptr);
    Camera camera;

    for (uint64_t frame = 1; frame <= 3; frame++)
    {
        const RenderSnapshot snapshot = renderer.ExtractSnapshot(camera, glm::mat4(1.0f), false);
        CHECK(snapshot.FrameNumber == frame);
        if (frame != 1)
            CHECK_FALSE(snapshot.CameraCut);
        CHECK(snapshot.ReleasedHistoryNamespaces.Empty());
    }
}

TEST_CASE("Scene camera history survives component-pool relocation", "[Memory][Frame][Renderer][SceneSync]")
{
    const Ref<Scene> scene = CreateRef<Scene>(false);
    Entity primaryCamera = scene->CreateEntity("Primary camera");
    primaryCamera.AddComponent<CameraComponent>();

    SceneRenderer renderer(scene, nullptr);
    RenderSnapshot snapshot;
    renderer.ExtractSnapshot(snapshot, false);
    const uint64_t historyNamespace = snapshot.HistoryNamespace;
    REQUIRE(historyNamespace != 0);
    REQUIRE(snapshot.CameraCut);

    Vector<Entity> temporaryCameras;
    temporaryCameras.reserve(1024);
    for (uint32_t index = 0; index < 1024; index++)
    {
        Entity camera = scene->CreateEntity("Temporary camera");
        camera.AddComponent<CameraComponent>();
        temporaryCameras.push_back(camera);
    }
    for (Entity camera : temporaryCameras)
        scene->DestroyEntity(camera);

    renderer.ExtractSnapshot(snapshot, false);
    CHECK(snapshot.HistoryNamespace == historyNamespace);
    CHECK_FALSE(snapshot.CameraCut);
    CHECK(snapshot.ReleasedHistoryNamespaces.Empty());
}

TEST_CASE("Camera-less frames retire inactive scene history", "[Memory][Frame][Renderer][SceneSync]")
{
    const Ref<Scene> scene = CreateRef<Scene>(false);
    Entity camera = scene->CreateEntity("Removed camera");
    camera.AddComponent<CameraComponent>();

    SceneRenderer renderer(scene, nullptr);
    RenderSnapshot snapshot;
    renderer.ExtractSnapshot(snapshot, false);
    const uint64_t historyNamespace = snapshot.HistoryNamespace;
    REQUIRE(historyNamespace != 0);

    scene->DestroyEntity(camera);
    for (uint32_t frame = 0; frame < 121; frame++)
    {
        snapshot.FrameNumber = frame + 2u;
        renderer.ExtractSnapshot(snapshot, false);
    }

    REQUIRE(snapshot.ReleasedHistoryNamespaces.Size() == 1);
    CHECK(snapshot.ReleasedHistoryNamespaces[0] == historyNamespace);
}

TEST_CASE("Renderer destruction releases history without another snapshot", "[Memory][Frame][Renderer][SceneSync]")
{
    const Ref<Scene> firstScene = CreateRef<Scene>(false);
    const Ref<Scene> secondScene = CreateRef<Scene>(false);
    RecordingHistoryReleaseSink releases;
    uint64_t firstNamespace = 0;

    {
        SceneRenderer renderer(firstScene, nullptr, &releases);
        RenderSnapshot snapshot;
        Camera camera;
        renderer.ExtractSnapshot(snapshot, camera, glm::mat4(1.0f), false);
        firstNamespace = snapshot.HistoryNamespace;
        REQUIRE(firstNamespace != 0);

        renderer.SetScene(secondScene);
        CHECK(releases.ReleasedNamespaces.empty());
    }

    REQUIRE(releases.ReleasedNamespaces.size() == 1);
    CHECK(releases.ReleasedNamespaces[0] == firstNamespace);
}

TEST_CASE("Thread allocation snapshots cover standard allocation families", "[Memory][Frame]")
{
    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();

    void* scalar = ::operator new(16);
    void* array = ::operator new[](32, std::nothrow);
    void* alignedScalar = ::operator new(64, std::align_val_t(64));
    void* alignedArray = ::operator new[](128, std::align_val_t(64), std::nothrow);

#if defined(__cpp_sized_deallocation)
    ::operator delete(scalar, size_t{ 16 });
#else
    ::operator delete(scalar);
#endif
    ::operator delete[](array, std::nothrow);
#if defined(__cpp_sized_deallocation)
    ::operator delete(alignedScalar, size_t{ 64 }, std::align_val_t(64));
#else
    ::operator delete(alignedScalar, std::align_val_t(64));
#endif
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

TEST_CASE("Physics contact collider pairs stay inline on the dispatch path", "[Memory][Frame][Physics]")
{
    constexpr uint32_t contactCount = 10'000;
    Ref<Scene> scene = CreateRef<Scene>(false);
    const Entity first = scene->CreateEntity("First collider");
    const Entity second = scene->CreateEntity("Second collider");

    uint64_t observedHandles = 0;
    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    for (uint32_t contact = 0; contact < contactCount; contact++)
    {
        // Box2D snapshots a pair, then copies it once for each scripted receiver.
        Collision2D snapshot2D;
        snapshot2D.Colliders = { first, second };
        Collision2D firstReceiver2D = snapshot2D;
        Collision2D secondReceiver2D = snapshot2D;
        std::swap(secondReceiver2D.Colliders[0], secondReceiver2D.Colliders[1]);

        // The 3D scene bridge constructs one collision payload for each receiver.
        Collision3D firstReceiver3D;
        firstReceiver3D.Colliders = { first, second };
        Collision3D secondReceiver3D;
        secondReceiver3D.Colliders = { second, first };

        observedHandles += static_cast<uint32_t>(firstReceiver2D.Colliders[0].GetHandle());
        observedHandles += static_cast<uint32_t>(secondReceiver2D.Colliders[0].GetHandle());
        observedHandles += static_cast<uint32_t>(firstReceiver3D.Colliders[0].GetHandle());
        observedHandles += static_cast<uint32_t>(secondReceiver3D.Colliders[0].GetHandle());
    }
    const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();
    const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, after);

    const uint64_t expectedPerContact = 2ull * static_cast<uint32_t>(first.GetHandle()) + 2ull * static_cast<uint32_t>(second.GetHandle());
    CHECK(observedHandles == expectedPerContact * contactCount);
    CHECK(delta.AllocationCount == 0u);
    CHECK(delta.RequestedBytes == 0u);
}

TEST_CASE("Render blackboard rebuilds allocate nothing after warm-up", "[Memory][Frame][Renderer][RenderGraph]")
{
    constexpr std::array<uint32_t, 3> resourceCounts{ 1u, 1000u, 10000u };
    constexpr uint32_t frameCount = 120u;
    constexpr StringView missingName = "CrownyFrameGraphResource_ThatDoesNotExist";

    for (const uint32_t resourceCount : resourceCounts)
    {
        RenderBlackboard blackboard;
        Vector<String> names;
        names.reserve(resourceCount);
        for (uint32_t index = 0; index < resourceCount; index++)
            names.push_back("CrownyFrameGraphResource_" + std::to_string(index));

        auto rebuild = [&]() {
            blackboard.Clear();
            for (uint32_t index = 0; index < resourceCount; index++)
            {
                blackboard.Set(StringView(names[index]), { index, 1u, RenderGraphResourceType::Buffer });
            }

            uint64_t checksum = 0;
            for (const String& name : names)
                checksum += static_cast<uint64_t>(blackboard.Get(StringView(name)).Index) + 1u;
            return checksum;
        };

        const uint64_t expectedChecksum = static_cast<uint64_t>(resourceCount) * (static_cast<uint64_t>(resourceCount) + 1u) / 2u;
        CHECK(rebuild() == expectedChecksum);
        blackboard.Clear();
        CHECK_FALSE(blackboard.Contains(StringView(names.front())));
        CHECK_FALSE(blackboard.Get(missingName).IsValid());
        CHECK(rebuild() == expectedChecksum);

        const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
        uint64_t checksum = 0;
        uint64_t misses = 0;
        for (uint32_t frame = 0; frame < frameCount; frame++)
        {
            checksum += rebuild();
            misses += blackboard.Contains(missingName) ? 0u : 1u;
            misses += blackboard.Get(missingName).IsValid() ? 0u : 1u;
        }
        const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();
        const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, after);

        INFO("Resource count: " << resourceCount);
        CHECK(checksum == expectedChecksum * frameCount);
        CHECK(misses == 2u * frameCount);
        CHECK(delta.AllocationCount == 0u);
        CHECK(delta.RequestedBytes == 0u);
    }
}

TEST_CASE("Scene light synchronization allocates nothing after warm-up", "[Memory][Frame][Renderer][SceneSync]")
{
    constexpr uint32_t lightCount = 1000;
    Ref<Scene> scene = CreateRef<Scene>(false);
    Vector<Entity> lights;
    lights.reserve(lightCount);
    for (uint32_t index = 0; index < lightCount; index++)
    {
        Entity entity = scene->CreateEntity("Light");
        LightComponent& light = entity.AddComponent<LightComponent>();
        light.Type = LightType::Point;
        light.Shadows.Mode = LightShadowMode::Disabled;
        lights.push_back(entity);
    }

    SceneRenderer renderer(scene, nullptr);
    RenderSnapshot snapshot;
    for (uint32_t warmup = 0; warmup < 4; warmup++)
        renderer.ExtractSnapshot(snapshot, false);

    const Memory::ThreadAllocationSnapshot beforeStable = Memory::GetThreadAllocationSnapshot();
    for (uint32_t frame = 0; frame < 120; frame++)
        renderer.ExtractSnapshot(snapshot, false);
    const Memory::ThreadAllocationSnapshot afterStable = Memory::GetThreadAllocationSnapshot();
    const Memory::ThreadAllocationSnapshot stableDelta = Memory::GetThreadAllocationDelta(beforeStable, afterStable);

    CHECK(snapshot.LegacyLights.Size() == lightCount);
    CHECK(stableDelta.AllocationCount == 0);
    CHECK(stableDelta.RequestedBytes == 0);

    constexpr uint32_t removedLightCount = lightCount / 2u;
    for (uint32_t index = 0; index < removedLightCount; index++)
        scene->DestroyEntity(lights[index]);
    renderer.ExtractSnapshot(snapshot, false);
    const size_t destroyChanges =
      static_cast<size_t>(std::count_if(snapshot.RenderLightChanges.begin(), snapshot.RenderLightChanges.end(),
                                        [](const RenderLightChange& change) { return change.Type == RenderLightChangeType::Destroy; }));
    CHECK(destroyChanges == removedLightCount);

    const Memory::ThreadAllocationSnapshot beforeReduced = Memory::GetThreadAllocationSnapshot();
    for (uint32_t frame = 0; frame < 120; frame++)
        renderer.ExtractSnapshot(snapshot, false);
    const Memory::ThreadAllocationSnapshot afterReduced = Memory::GetThreadAllocationSnapshot();
    const Memory::ThreadAllocationSnapshot reducedDelta = Memory::GetThreadAllocationDelta(beforeReduced, afterReduced);

    CHECK(snapshot.LegacyLights.Size() == lightCount - removedLightCount);
    CHECK(reducedDelta.AllocationCount == 0);
    CHECK(reducedDelta.RequestedBytes == 0);
}

TEST_CASE("Hierarchy transform propagation allocates nothing after warm-up", "[Memory][Frame][Ecs][Hierarchy]")
{
    constexpr std::array<uint32_t, 3> entityCounts{ 1u, 1000u, 10000u };
    constexpr uint32_t frameCount = 120u;

    for (const uint32_t entityCount : entityCounts)
    {
        Ref<Scene> scene = CreateRef<Scene>(false);
        Entity root = scene->CreateEntity("Root");
        for (uint32_t index = 1u; index < entityCount; index++)
        {
            Entity child = scene->CreateEntity("Child");
            REQUIRE(child.SetParent(root));
        }

        const auto propagate = [&](uint32_t frame) {
            auto scope = scene->DeferTransformChanges();
            root.SetLocalTransform(Transform({ static_cast<float>(frame), 0.0f, 0.0f }, glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                               { 1.0f, 1.0f, 1.0f }),
                                   false);
        };
        propagate(0u);
        propagate(1u);
        propagate(2u);

        const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
        for (uint32_t frame = 0u; frame < frameCount; frame++)
            propagate(frame + 3u);
        const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();
        const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, after);

        INFO("Entity count: " << entityCount);
        CHECK(scene->GetLastTransformPropagationStats().VisitedEntityCount == entityCount);
        CHECK(delta.AllocationCount == 0u);
        CHECK(delta.RequestedBytes == 0u);
    }
}

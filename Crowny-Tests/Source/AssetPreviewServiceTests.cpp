#include <catch2/catch_test_macros.hpp>

#include "Editor/AssetPreviewService.h"

#include "Crowny/Common/Uuid.h"
#include "Crowny/Threading/TaskSystem.h"

#include <chrono>
#include <condition_variable>
#include <limits>
#include <mutex>

using namespace Crowny;

namespace
{
    using namespace std::chrono_literals;

    class TaskSystemLease
    {
    public:
        TaskSystemLease()
        {
            if (!TaskSystem::IsStartedUp())
            {
                TaskSystem::StartUp(2);
                m_OwnsTaskSystem = true;
            }
        }

        ~TaskSystemLease()
        {
            if (!m_OwnsTaskSystem)
                return;
            TaskSystem::Get().Drain();
            TaskSystem::Shutdown();
        }

    private:
        bool m_OwnsTaskSystem = false;
    };

    class WorkerGate
    {
    public:
        ~WorkerGate() { Open(); }

        void EnterAndWait()
        {
            std::unique_lock lock(m_Mutex);
            m_Entered++;
            m_Changed.notify_all();
            m_Changed.wait(lock, [this]() { return m_Open; });
        }

        bool WaitForEntered(uint32_t count)
        {
            std::unique_lock lock(m_Mutex);
            return m_Changed.wait_for(lock, 5s, [this, count]() { return m_Entered >= count; });
        }

        void Open()
        {
            std::lock_guard lock(m_Mutex);
            m_Open = true;
            m_Changed.notify_all();
        }

    private:
        std::mutex m_Mutex;
        std::condition_variable m_Changed;
        uint32_t m_Entered = 0;
        bool m_Open = false;
    };

    FileEntry MakeAudioEntry()
    {
        FileEntry entry;
        entry.Filepath = "never-read-preview.wav";
        entry.Filesize = 32;
        entry.LastUpdateTime = 1;
        entry.Metadata = CreateRef<AssetMetadata>();
        entry.Metadata->Uuid = UuidGenerator::Generate();
        entry.Metadata->Type = AssetType::AudioClip;
        return entry;
    }
} // namespace

TEST_CASE("Stale queued previews release their scheduler slots", "[Editor][Assets][Preview]")
{
    TaskSystemLease taskSystemLease;
    WorkerGate gate;
    AssetPreviewService previews;

    const uint32_t workerCount = TaskSystem::Get().GetWorkerCount();
    REQUIRE(workerCount > 0);

    Vector<Ref<Task>> blockers;
    blockers.reserve(workerCount);
    TaskOptions blockerOptions;
    blockerOptions.Priority = TaskPriority::High;
    for (uint32_t index = 0; index < workerCount; index++)
        blockers.push_back(TaskSystem::Get().Submit("Asset preview test blocker", [&gate]() { gate.EnterAndWait(); }, blockerOptions));
    REQUIRE(gate.WaitForEntered(workerCount));

    FileEntry entry = MakeAudioEntry();
    constexpr uint64_t REVISION_COUNT = 160;
    for (uint64_t revision = 1; revision <= REVISION_COUNT; revision++)
    {
        entry.Revision = revision;
        const AssetPreviewResult* result = previews.Request(entry, 64);
        REQUIRE(result != nullptr);
        REQUIRE(result->Error.empty());
        previews.Update();
    }

    previews.Clear();
    gate.Open();
    for (const Ref<Task>& blocker : blockers)
        blocker->Wait();

    TaskOptions markerOptions;
    markerOptions.Priority = TaskPriority::Low;
    TaskSystem::Get().Submit("Asset preview test queue marker", []() {}, markerOptions)->Wait();
}

TEST_CASE("Oversized preview requests share the bounded cache entry", "[Editor][Assets][Preview]")
{
    AssetPreviewService previews;
    const FileEntry entry = MakeAudioEntry();

    const AssetPreviewResult* oversized = previews.Request(entry, std::numeric_limits<uint32_t>::max());
    const AssetPreviewResult* maximum = previews.Request(entry, 256);

    REQUIRE(oversized != nullptr);
    CHECK(maximum == oversized);
    CHECK(oversized->Status == AssetPreviewStatus::Queued);
}

TEST_CASE("Mesh preview shading stays readable on every facet orientation", "[Editor][Assets][Preview]")
{
    const auto luminance = [](const glm::vec4& color) { return 0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b; };

    const glm::vec4 towardsKey = ShadeMeshPreviewFacet(glm::vec3(-0.45f, -0.55f, 0.70f));
    const glm::vec4 awayFromKey = ShadeMeshPreviewFacet(glm::vec3(0.45f, 0.55f, 0.70f));
    const glm::vec4 backFacing = ShadeMeshPreviewFacet(glm::vec3(0.45f, 0.55f, -0.70f));
    const glm::vec4 degenerate = ShadeMeshPreviewFacet(glm::vec3(0.0f));

    CHECK(luminance(towardsKey) > luminance(awayFromKey));
    CHECK(luminance(awayFromKey) > 0.3f); // No facet may fall back into the old near-black range.
    CHECK(luminance(towardsKey) <= 1.0f);
    CHECK(backFacing == towardsKey); // Winding does not change the shade: a flipped facet shades like its mirror.
    CHECK(degenerate.a == 1.0f);
    for (int component = 0; component < 3; component++)
    {
        CHECK(towardsKey[component] >= 0.0f);
        CHECK(towardsKey[component] <= 1.0f);
    }

    CHECK(ShouldOutlineMeshPreview(12));
    CHECK(ShouldOutlineMeshPreview(2500));
    CHECK_FALSE(ShouldOutlineMeshPreview(0));
    CHECK_FALSE(ShouldOutlineMeshPreview(100000));

    const glm::vec4 top = GetMeshPreviewBackground(0.0f);
    const glm::vec4 bottom = GetMeshPreviewBackground(1.0f);
    CHECK(luminance(top) > luminance(bottom));
    CHECK(luminance(bottom) > 0.1f);
    CHECK(luminance(awayFromKey) > luminance(top)); // The mesh always separates from its background.
}

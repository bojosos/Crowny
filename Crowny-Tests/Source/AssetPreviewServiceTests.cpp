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

    FileEntry MakePreviewEntry(AssetType type, const Path& path)
    {
        FileEntry entry;
        entry.Filepath = path;
        entry.Filesize = 32;
        entry.LastUpdateTime = 1;
        entry.Metadata = CreateRef<AssetMetadata>();
        entry.Metadata->Uuid = UuidGenerator::Generate();
        entry.Metadata->Type = type;
        return entry;
    }

    FileEntry MakeAudioEntry() { return MakePreviewEntry(AssetType::AudioClip, "never-read-preview.wav"); }
} // namespace

TEST_CASE("Preview cache reserves a bounded amount for every supported asset kind", "[Editor][Assets][Preview]")
{
    constexpr uint32_t PREVIEW_SIZE = 64;
    constexpr size_t PREVIEW_BYTES = static_cast<size_t>(PREVIEW_SIZE) * PREVIEW_SIZE * 4u;
    AssetPreviewService previews(PREVIEW_BYTES * 2u);

    const FileEntry texture = MakePreviewEntry(AssetType::Texture, "never-read-preview.png");
    const FileEntry mesh = MakePreviewEntry(AssetType::MeshSource, "never-read-preview.obj");
    const FileEntry audio = MakeAudioEntry();

    REQUIRE(previews.Request(texture, PREVIEW_SIZE) != nullptr);
    REQUIRE(previews.Request(mesh, PREVIEW_SIZE) != nullptr);
    const AssetPreviewResult* rejected = previews.Request(audio, PREVIEW_SIZE);

    REQUIRE(rejected != nullptr);
    CHECK_FALSE(rejected->Error.empty());
    const AssetPreviewCacheStats full = previews.GetStats();
    CHECK(full.Entries == 2);
    CHECK(full.ReservedBytes == PREVIEW_BYTES * 2u);
    CHECK(full.Pending == 2);
    CHECK(full.Running == 0);

    previews.CancelPending();
    const AssetPreviewCacheStats canceled = previews.GetStats();
    CHECK(canceled.Entries == 0);
    CHECK(canceled.ReservedBytes == 0);
    CHECK(canceled.Pending == 0);
    CHECK(canceled.Running == 0);
}

TEST_CASE("Preview invalidation and clear release cache reservations", "[Editor][Assets][Preview]")
{
    constexpr uint32_t PREVIEW_SIZE = 32;
    constexpr size_t PREVIEW_BYTES = static_cast<size_t>(PREVIEW_SIZE) * PREVIEW_SIZE * 4u;
    AssetPreviewService previews(PREVIEW_BYTES * 2u);

    const FileEntry texture = MakePreviewEntry(AssetType::EnvironmentMap, "never-read-preview.hdr");
    const FileEntry audio = MakeAudioEntry();
    REQUIRE(previews.Request(texture, PREVIEW_SIZE) != nullptr);
    REQUIRE(previews.Request(audio, PREVIEW_SIZE) != nullptr);

    previews.Invalidate(texture.Metadata->Uuid);
    const AssetPreviewCacheStats invalidated = previews.GetStats();
    CHECK(invalidated.Entries == 1);
    CHECK(invalidated.ReservedBytes == PREVIEW_BYTES);
    CHECK(invalidated.Pending == 1);

    previews.Clear();
    const AssetPreviewCacheStats cleared = previews.GetStats();
    CHECK(cleared.Entries == 0);
    CHECK(cleared.ReservedBytes == 0);
    CHECK(cleared.Pending == 0);
    CHECK(cleared.Running == 0);
}

TEST_CASE("Changing preview asset type replaces blocked work for the same asset", "[Editor][Assets][Preview]")
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
        blockers.push_back(TaskSystem::Get().Submit("Asset preview type blocker", [&gate]() { gate.EnterAndWait(); }, blockerOptions));
    REQUIRE(gate.WaitForEntered(workerCount));

    FileEntry entry = MakeAudioEntry();
    const AssetPreviewResult* audio = previews.Request(entry, 64);
    REQUIRE(audio != nullptr);
    previews.Update();

    entry.Metadata->Type = AssetType::Texture;
    const AssetPreviewResult* texture = previews.Request(entry, 64);
    REQUIRE(texture != nullptr);
    CHECK(texture != audio);
    const AssetPreviewCacheStats replaced = previews.GetStats();
    CHECK(replaced.Entries == 1);
    CHECK(replaced.Pending == 1);
    CHECK(replaced.Running == 1);

    gate.Open();
    previews.Clear();
    for (const Ref<Task>& blocker : blockers)
        blocker->Wait();
}

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

    const AssetPreviewCacheStats saturated = previews.GetStats();
    CHECK(saturated.Entries == 1);
    CHECK(saturated.Pending <= 1);
    CHECK(saturated.Running <= 2);

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

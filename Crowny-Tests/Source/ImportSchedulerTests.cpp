#include <catch2/catch_test_macros.hpp>

#include "Editor/AssetLibraryServices.h"

#include "Crowny/Common/Log.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Threading/TaskSystem.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>

using namespace Crowny;

namespace
{
    using namespace std::chrono_literals;

    class TestGate
    {
    public:
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

    class SubmitHookGuard
    {
    public:
        explicit SubmitHookGuard(std::function<void()> hook) { TaskSystemTestAccess::SetBeforeSubmit(TaskSystem::Get(), std::move(hook)); }

        ~SubmitHookGuard() { Reset(); }

        void Reset()
        {
            if (!m_Active)
                return;
            m_Active = false;
            if (TaskSystem::IsStartedUp())
                TaskSystemTestAccess::SetBeforeSubmit(TaskSystem::Get(), {});
        }

    private:
        bool m_Active = true;
    };

    class TestAsset final : public Asset
    {
    public:
        explicit TestAsset(std::atomic<uint32_t>* initCount = nullptr, std::thread::id* initThread = nullptr)
          : m_InitCount(initCount), m_InitThread(initThread)
        {
        }

        void Init() override
        {
            if (m_InitThread != nullptr)
                *m_InitThread = std::this_thread::get_id();
            if (m_InitCount != nullptr)
                m_InitCount->fetch_add(1, std::memory_order_relaxed);
        }

        AssetType GetAssetType() const override { return AssetType::PlainText; }

    private:
        std::atomic<uint32_t>* m_InitCount;
        std::thread::id* m_InitThread;
    };

    class ControlledImporter final : public SpecificImporter
    {
    public:
        using Handler = std::function<Ref<Asset>(const Path&)>;

        ControlledImporter(String extension, ImporterThreadingPolicy policy, Handler handler)
          : m_Extension(std::move(extension)), m_Policy(policy), m_Handler(std::move(handler))
        {
        }

        bool IsExtensionSupported(const String& extension) const override { return extension == m_Extension; }
        bool IsMagicNumSupported(uint8_t*, uint32_t) const override { return false; }

        Ref<Asset> Import(const Path& path, Ref<const ImportOptions>) override
        {
            const uint32_t active = m_Active.fetch_add(1, std::memory_order_acq_rel) + 1;
            uint32_t maximum = m_MaximumActive.load(std::memory_order_relaxed);
            while (maximum < active && !m_MaximumActive.compare_exchange_weak(maximum, active, std::memory_order_relaxed))
            {
            }

            {
                std::lock_guard lock(m_CallMutex);
                m_Calls[path.filename().string()]++;
                m_CallThreads.push_back(std::this_thread::get_id());
            }

            struct ActiveCall
            {
                ~ActiveCall() { Counter.fetch_sub(1, std::memory_order_acq_rel); }
                std::atomic<uint32_t>& Counter;
            } activeCall{ m_Active };
            return m_Handler(path);
        }

        Ref<ImportOptions> CreateImportOptions() const override { return CreateRef<ImportOptions>(); }
        ImporterThreadingPolicy GetThreadingPolicy() const override { return m_Policy; }

        uint32_t GetCallCount() const
        {
            std::lock_guard lock(m_CallMutex);
            uint32_t count = 0;
            for (const auto& [name, calls] : m_Calls)
                count += calls;
            return count;
        }

        uint32_t GetCallCount(const String& filename) const
        {
            std::lock_guard lock(m_CallMutex);
            const auto found = m_Calls.find(filename);
            return found != m_Calls.end() ? found->second : 0;
        }

        uint32_t GetMaximumActive() const { return m_MaximumActive.load(std::memory_order_acquire); }

        Vector<std::thread::id> GetCallThreads() const
        {
            std::lock_guard lock(m_CallMutex);
            return m_CallThreads;
        }

    private:
        String m_Extension;
        ImporterThreadingPolicy m_Policy;
        Handler m_Handler;
        std::atomic<uint32_t> m_Active{ 0 };
        std::atomic<uint32_t> m_MaximumActive{ 0 };
        mutable std::mutex m_CallMutex;
        UnorderedMap<String, uint32_t> m_Calls;
        Vector<std::thread::id> m_CallThreads;
    };

    class SchedulerFixture
    {
    public:
        SchedulerFixture()
        {
            Log::Init("CrownyTests");
            if (!TaskSystem::IsStartedUp())
            {
                TaskSystem::StartUp(3);
                m_OwnsTaskSystem = true;
            }
            if (!Importer::IsStartedUp())
            {
                Importer::StartUp();
                m_OwnsImporter = true;
            }

            const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
            m_TemporaryDirectory = std::filesystem::temp_directory_path() / ("crowny_import_scheduler_" + std::to_string(unique));
            std::filesystem::create_directories(m_TemporaryDirectory);
        }

        ~SchedulerFixture()
        {
            if (m_OwnsTaskSystem && TaskSystem::IsStartedUp())
                TaskSystem::Get().Drain();
            if (m_OwnsImporter)
                Importer::Shutdown();
            if (m_OwnsTaskSystem)
                TaskSystem::Shutdown();
            std::error_code error;
            std::filesystem::remove_all(m_TemporaryDirectory, error);
        }

        Path CreateFile(const String& filename)
        {
            const Path path = m_TemporaryDirectory / filename;
            std::ofstream stream(path, std::ios::binary);
            stream << filename;
            return path;
        }

        ImportTask CreateTask(const Path& path)
        {
            Ref<FileEntry> entry = CreateRef<FileEntry>();
            entry->Filepath = path;
            ImportTask task;
            task.Entry = entry;
            return task;
        }

        ControlledImporter* RegisterImporter(String extension, ImporterThreadingPolicy policy, ControlledImporter::Handler handler)
        {
            auto* importer = new ControlledImporter(extension, policy, std::move(handler));
            Importer::Get().RegisterImporter(importer, { extension });
            return importer;
        }

        static void Drain(ImportScheduler& scheduler, const ImportScheduler::CompletionHandler& completionHandler)
        {
            const auto deadline = std::chrono::steady_clock::now() + 5s;
            while (std::chrono::steady_clock::now() < deadline)
            {
                if (scheduler.ProcessCompleted(completionHandler, std::numeric_limits<uint32_t>::max()))
                    return;
                std::this_thread::yield();
            }
            FAIL("Import scheduler did not finish before the test deadline");
        }

    private:
        Path m_TemporaryDirectory;
        bool m_OwnsTaskSystem = false;
        bool m_OwnsImporter = false;
    };
} // namespace

TEST_CASE_METHOD(SchedulerFixture, "Main-thread-only imports use main-thread import and publication", "[Editor][ImportScheduler]")
{
    const std::thread::id mainThread = std::this_thread::get_id();
    std::atomic<uint32_t> initCount{ 0 };
    std::thread::id initThread;
    ControlledImporter* importer =
      RegisterImporter("main", ImporterThreadingPolicy::MainThreadOnly, [&](const Path&) { return CreateRef<TestAsset>(&initCount, &initThread); });

    ImportScheduler scheduler;
    scheduler.Schedule({ CreateTask(CreateFile("asset.main")) });

    uint32_t completionCount = 0;
    Drain(scheduler, [&](const ImportResult& result) {
        CHECK(result.Task.RunOnMainThread);
        CHECK(result.Status == ImportResultStatus::Succeeded);
        CHECK(std::this_thread::get_id() == mainThread);
        CHECK(initCount.load(std::memory_order_acquire) == 1);
        completionCount++;
    });

    REQUIRE(importer->GetCallThreads().size() == 1);
    CHECK(importer->GetCallThreads().front() == mainThread);
    CHECK(initThread == mainThread);
    CHECK(completionCount == 1);
}

TEST_CASE_METHOD(SchedulerFixture, "Parallel imports obey the configured worker-lane cap", "[Editor][ImportScheduler]")
{
    TestGate gate;
    ControlledImporter* importer = RegisterImporter("parallel", ImporterThreadingPolicy::ParallelWorker, [&](const Path&) {
        gate.EnterAndWait();
        return CreateRef<TestAsset>();
    });

    ImportScheduler scheduler(2);
    Vector<ImportTask> tasks;
    for (uint32_t index = 0; index < 6; index++)
        tasks.push_back(CreateTask(CreateFile("asset" + std::to_string(index) + ".parallel")));
    scheduler.Schedule(std::move(tasks));

    const uint32_t laneLimit = scheduler.GetWorkerLaneLimit();
    REQUIRE(laneLimit > 0);
    if (TaskSystem::Get().GetWorkerCount() > 1)
        CHECK(laneLimit < TaskSystem::Get().GetWorkerCount());
    REQUIRE(gate.WaitForEntered(laneLimit));
    CHECK(importer->GetMaximumActive() == laneLimit);
    CHECK(importer->GetMaximumActive() <= 2);

    gate.Open();
    uint32_t completionCount = 0;
    Drain(scheduler, [&](const ImportResult&) { completionCount++; });
    CHECK(importer->GetCallCount() == 6);
    CHECK(completionCount == 6);
    if (laneLimit > 1)
        CHECK(importer->GetMaximumActive() > 1);
}

TEST_CASE_METHOD(SchedulerFixture, "Serialized-worker imports never overlap for one importer", "[Editor][ImportScheduler]")
{
    TestGate firstCallGate;
    std::atomic<uint32_t> callIndex{ 0 };
    ControlledImporter* importer = RegisterImporter("serialized", ImporterThreadingPolicy::SerializedWorker, [&](const Path&) {
        if (callIndex.fetch_add(1, std::memory_order_acq_rel) == 0)
            firstCallGate.EnterAndWait();
        return CreateRef<TestAsset>();
    });

    ImportScheduler scheduler(2);
    Vector<ImportTask> tasks;
    for (uint32_t index = 0; index < 4; index++)
        tasks.push_back(CreateTask(CreateFile("asset" + std::to_string(index) + ".serialized")));
    scheduler.Schedule(std::move(tasks));

    REQUIRE(firstCallGate.WaitForEntered(1));
    firstCallGate.Open();
    Drain(scheduler, [](const ImportResult&) {});
    CHECK(importer->GetCallCount() == 4);
    CHECK(importer->GetMaximumActive() == 1);
}

TEST_CASE_METHOD(SchedulerFixture, "Import results publish once in input order including failures", "[Editor][ImportScheduler]")
{
    TestGate firstCallGate;
    std::mutex laterMutex;
    std::condition_variable laterChanged;
    uint32_t laterCompleted = 0;
    ControlledImporter* importer = RegisterImporter("ordered", ImporterThreadingPolicy::ParallelWorker, [&](const Path& path) -> Ref<Asset> {
        const String filename = path.filename().string();
        if (filename == "asset0.ordered")
            firstCallGate.EnterAndWait();
        else
        {
            std::lock_guard lock(laterMutex);
            laterCompleted++;
            laterChanged.notify_all();
        }
        return filename == "asset1.ordered" ? nullptr : CreateRef<TestAsset>();
    });

    ImportScheduler scheduler(2);
    Vector<ImportTask> tasks;
    for (uint32_t index = 0; index < 3; index++)
        tasks.push_back(CreateTask(CreateFile("asset" + std::to_string(index) + ".ordered")));
    scheduler.Schedule(std::move(tasks));

    Vector<uint64_t> sequences;
    Vector<ImportResultStatus> statuses;
    const uint32_t laneLimit = scheduler.GetWorkerLaneLimit();
    REQUIRE(laneLimit > 0);
    const bool firstCallEntered = firstCallGate.WaitForEntered(1);
    if (!firstCallEntered)
        firstCallGate.Open();
    REQUIRE(firstCallEntered);

    if (laneLimit > 1)
    {
        bool laterCallsCompleted = false;
        {
            std::unique_lock lock(laterMutex);
            laterCallsCompleted = laterChanged.wait_for(lock, 5s, [&]() { return laterCompleted == 2; });
        }
        if (!laterCallsCompleted)
            firstCallGate.Open();
        REQUIRE(laterCallsCompleted);

        const ImportProgress partialProgress = scheduler.GetProgress();
        CHECK(partialProgress.Active);
        CHECK(partialProgress.TotalFiles == 3);
        CHECK(partialProgress.CompletedFiles == 2);
        CHECK_FALSE(scheduler.ProcessCompleted([&](const ImportResult& result) { sequences.push_back(result.Task.Sequence); }));
        CHECK(sequences.empty());
    }

    firstCallGate.Open();
    Drain(scheduler, [&](const ImportResult& result) {
        sequences.push_back(result.Task.Sequence);
        statuses.push_back(result.Status);
    });

    CHECK(sequences == Vector<uint64_t>{ 0, 1, 2 });
    CHECK(statuses == Vector<ImportResultStatus>{ ImportResultStatus::Succeeded, ImportResultStatus::Failed, ImportResultStatus::Succeeded });
    CHECK(importer->GetCallCount("asset0.ordered") == 1);
    CHECK(importer->GetCallCount("asset1.ordered") == 1);
    CHECK(importer->GetCallCount("asset2.ordered") == 1);
    const ImportProgress finalProgress = scheduler.GetProgress();
    CHECK_FALSE(finalProgress.Active);
    CHECK(finalProgress.CompletedFiles == 3);
    CHECK(finalProgress.TotalFiles == 3);
}

TEST_CASE_METHOD(SchedulerFixture, "Shutdown waits for active import lanes and discards callbacks", "[Editor][ImportScheduler]")
{
    TestGate gate;
    RegisterImporter("shutdown", ImporterThreadingPolicy::ParallelWorker, [&](const Path&) {
        gate.EnterAndWait();
        return CreateRef<TestAsset>();
    });

    ImportScheduler scheduler(2);
    scheduler.Schedule({ CreateTask(CreateFile("asset.shutdown")) });
    REQUIRE(gate.WaitForEntered(1));

    std::future<void> shutdown = std::async(std::launch::async, [&]() { scheduler.Shutdown(); });
    CHECK(shutdown.wait_for(20ms) == std::future_status::timeout);
    gate.Open();
    shutdown.get();

    std::atomic<uint32_t> callbacks{ 0 };
    CHECK_FALSE(scheduler.ProcessCompleted([&](const ImportResult&) { callbacks.fetch_add(1, std::memory_order_relaxed); }));
    CHECK(callbacks.load(std::memory_order_acquire) == 0);
    CHECK_FALSE(scheduler.IsActive());
    CHECK(scheduler.GetProgress().TotalFiles == 0);
}

TEST_CASE_METHOD(SchedulerFixture, "Empty, single-file, and repeated import batches complete", "[Editor][ImportScheduler]")
{
    ControlledImporter* importer =
      RegisterImporter("repeat", ImporterThreadingPolicy::ParallelWorker, [](const Path&) { return CreateRef<TestAsset>(); });
    ImportScheduler scheduler(2);

    scheduler.Schedule({});
    CHECK_FALSE(scheduler.IsActive());
    CHECK(scheduler.GetProgress().TotalFiles == 0);

    Vector<uint64_t> sequences;
    scheduler.Schedule({ CreateTask(CreateFile("first.repeat")) });
    Drain(scheduler, [&](const ImportResult& result) { sequences.push_back(result.Task.Sequence); });
    scheduler.Schedule({ CreateTask(CreateFile("second.repeat")) });
    Drain(scheduler, [&](const ImportResult& result) { sequences.push_back(result.Task.Sequence); });

    CHECK(sequences == Vector<uint64_t>{ 0, 0 });
    CHECK(importer->GetCallCount() == 2);
}

TEST_CASE_METHOD(SchedulerFixture, "Progress snapshots remain consistent during parallel imports", "[Editor][ImportScheduler]")
{
    TestGate gate;
    RegisterImporter("progress", ImporterThreadingPolicy::ParallelWorker, [&](const Path&) {
        gate.EnterAndWait();
        return CreateRef<TestAsset>();
    });

    ImportScheduler scheduler(2);
    scheduler.Schedule({ CreateTask(CreateFile("first.progress")), CreateTask(CreateFile("second.progress")) });
    const uint32_t laneLimit = scheduler.GetWorkerLaneLimit();
    REQUIRE(gate.WaitForEntered(laneLimit));

    std::atomic<bool> stopReader{ false };
    std::atomic<bool> snapshotsValid{ true };
    std::thread reader([&]() {
        while (!stopReader.load(std::memory_order_acquire))
        {
            const ImportProgress progress = scheduler.GetProgress();
            if (progress.CompletedFiles > progress.TotalFiles || progress.GetFraction() < 0.0f || progress.GetFraction() > 1.0f)
                snapshotsValid.store(false, std::memory_order_release);
            std::this_thread::yield();
        }
    });

    gate.Open();
    Drain(scheduler, [](const ImportResult&) {});
    stopReader.store(true, std::memory_order_release);
    reader.join();
    CHECK(snapshotsValid.load(std::memory_order_acquire));
    CHECK(scheduler.GetProgress().CompletedFiles == 2);
}

TEST_CASE_METHOD(SchedulerFixture, "Rejected worker-lane submission publishes failed imports", "[Editor][ImportScheduler]")
{
    ControlledImporter* importer =
      RegisterImporter("rejected", ImporterThreadingPolicy::ParallelWorker, [](const Path&) { return CreateRef<TestAsset>(); });
    std::atomic<uint32_t> submitAttempts{ 0 };
    SubmitHookGuard submitHook([&]() {
        submitAttempts.fetch_add(1, std::memory_order_acq_rel);
        throw std::runtime_error("forced lane rejection");
    });

    ImportScheduler scheduler(2);
    scheduler.Schedule({ CreateTask(CreateFile("first.rejected")), CreateTask(CreateFile("second.rejected")) });
    submitHook.Reset();

    Vector<ImportResultStatus> statuses;
    Drain(scheduler, [&](const ImportResult& result) { statuses.push_back(result.Status); });

    CHECK(submitAttempts.load(std::memory_order_acquire) == 1);
    CHECK(statuses == Vector<ImportResultStatus>{ ImportResultStatus::Failed, ImportResultStatus::Failed });
    CHECK(importer->GetCallCount() == 0);
    CHECK(scheduler.GetWorkerLaneLimit() == 0);
}

TEST_CASE_METHOD(SchedulerFixture, "Partially accepted worker lanes roll the batch back to failed imports", "[Editor][ImportScheduler]")
{
    RegisterImporter("partial", ImporterThreadingPolicy::ParallelWorker, [](const Path&) { return CreateRef<TestAsset>(); });

    std::atomic<uint32_t> submitAttempts{ 0 };
    SubmitHookGuard submitHook([&]() {
        if (submitAttempts.fetch_add(1, std::memory_order_acq_rel) + 1 == 2)
            throw std::runtime_error("forced second-lane rejection");
    });

    ImportScheduler scheduler(2);
    Vector<ImportTask> tasks;
    for (uint32_t index = 0; index < 4; index++)
        tasks.push_back(CreateTask(CreateFile("asset" + std::to_string(index) + ".partial")));
    scheduler.Schedule(std::move(tasks));
    submitHook.Reset();

    Vector<ImportResultStatus> statuses;
    Drain(scheduler, [&](const ImportResult& result) { statuses.push_back(result.Status); });

    CHECK(submitAttempts.load(std::memory_order_acquire) == 2);
    CHECK(statuses == Vector<ImportResultStatus>(4, ImportResultStatus::Failed));
    CHECK(scheduler.GetWorkerLaneLimit() == 0);
}

TEST_CASE_METHOD(SchedulerFixture, "Throwing importers publish failed results without escaping", "[Editor][ImportScheduler]")
{
    ControlledImporter* mainThreadImporter =
      RegisterImporter("throwmain", ImporterThreadingPolicy::MainThreadOnly, [](const Path&) -> Ref<Asset> { throw std::runtime_error("main"); });
    ControlledImporter* workerImporter =
      RegisterImporter("throwworker", ImporterThreadingPolicy::ParallelWorker, [](const Path&) -> Ref<Asset> { throw std::runtime_error("worker"); });

    ImportScheduler scheduler(2);
    scheduler.Schedule({ CreateTask(CreateFile("first.throwmain")), CreateTask(CreateFile("second.throwworker")) });

    Vector<ImportResultStatus> statuses;
    Drain(scheduler, [&](const ImportResult& result) { statuses.push_back(result.Status); });

    CHECK(statuses == Vector<ImportResultStatus>{ ImportResultStatus::Failed, ImportResultStatus::Failed });
    CHECK(mainThreadImporter->GetCallCount() == 1);
    CHECK(workerImporter->GetCallCount() == 1);
}

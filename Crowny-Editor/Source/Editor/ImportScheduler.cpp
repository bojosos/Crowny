#include "cwepch.h"

#include "Editor/AssetLibraryServices.h"

#include "Crowny/Import/Importer.h"
#include "Crowny/Threading/TaskSystem.h"

#include <tracy/Tracy.hpp>

namespace Crowny
{
    namespace
    {
        enum class ScheduledImportState
        {
            PendingWorker,
            RunningWorker,
            PendingMainThread,
            RunningMainThread,
            Ready,
            Published
        };
    } // namespace

    struct ImportScheduler::BatchState
    {
        struct Item
        {
            ImportResult Result;
            SpecificImporter* Importer = nullptr;
            ImporterThreadingPolicy Policy = ImporterThreadingPolicy::MainThreadOnly;
            std::shared_ptr<Mutex> SerializedImporterMutex;
            std::chrono::steady_clock::time_point QueuedAt;
            ScheduledImportState State = ScheduledImportState::PendingMainThread;
        };

        mutable Mutex Mutex;
        Vector<Item> Items;
        Vector<uint32_t> WorkerItemIndices;
        Vector<Ref<Task>> LaneTasks;
        std::atomic<uint32_t> NextWorkerItem{ 0 };
        std::atomic<bool> CancelRequested{ false };
        uint32_t NextPublishSequence = 0;
        uint32_t WorkerLaneLimit = 0;
        ImportProgress Progress;
    };

    ImportScheduler::ImportScheduler(uint32_t maxWorkerLanes) : m_MaxWorkerLanes(std::max(maxWorkerLanes, 1u)) {}

    ImportScheduler::~ImportScheduler() { Shutdown(); }

    void ImportScheduler::Schedule(Vector<ImportTask> tasks)
    {
        ZoneScopedN("ImportScheduler::Batch");
        ZoneValue(tasks.size());

        Shutdown();
        if (tasks.empty())
            return;

        const std::shared_ptr<BatchState> batch = std::make_shared<BatchState>();
        batch->Items.reserve(tasks.size());
        batch->WorkerItemIndices.reserve(tasks.size());
        batch->Progress.Active = true;
        batch->Progress.TotalFiles = static_cast<uint32_t>(tasks.size());

        Importer* const importerRegistry = Importer::TryGet();
        TaskSystem* const taskSystem = TaskSystem::TryGet();
        UnorderedMap<SpecificImporter*, std::shared_ptr<Mutex>> serializedImporterMutexes;
        const auto queuedAt = std::chrono::steady_clock::now();

        for (uint32_t sequence = 0; sequence < static_cast<uint32_t>(tasks.size()); sequence++)
        {
            ImportTask& task = tasks[sequence];
            task.Sequence = sequence;
            if (task.Entry != nullptr)
                task.SourcePath = task.Entry->Filepath;

            BatchState::Item item;
            item.QueuedAt = queuedAt;
            item.Result.Task = std::move(task);
            item.Importer = importerRegistry != nullptr ? importerRegistry->GetImporterForFile(item.Result.Task.SourcePath) : nullptr;

            if (item.Importer != nullptr)
            {
                item.Result.Task.Options = item.Result.Task.Options != nullptr ? item.Result.Task.Options->Clone()
                                                                                : item.Importer->CreateImportOptions();
                item.Policy = taskSystem != nullptr ? item.Importer->GetThreadingPolicy() : ImporterThreadingPolicy::MainThreadOnly;
            }

            item.Result.Task.RunOnMainThread = item.Policy == ImporterThreadingPolicy::MainThreadOnly;
            if (item.Importer == nullptr || item.Result.Task.Options == nullptr)
            {
                item.State = ScheduledImportState::Ready;
                item.Result.Status = ImportResultStatus::Failed;
                batch->Progress.CompletedFiles++;
            }
            else if (item.Policy == ImporterThreadingPolicy::MainThreadOnly)
                item.State = ScheduledImportState::PendingMainThread;
            else
            {
                item.State = ScheduledImportState::PendingWorker;
                if (item.Policy == ImporterThreadingPolicy::SerializedWorker)
                {
                    auto& importerMutex = serializedImporterMutexes[item.Importer];
                    if (importerMutex == nullptr)
                        importerMutex = std::make_shared<Mutex>();
                    item.SerializedImporterMutex = importerMutex;
                }
                batch->WorkerItemIndices.push_back(sequence);
            }
            batch->Items.push_back(std::move(item));
        }

        if (!batch->WorkerItemIndices.empty())
        {
            const uint32_t taskSystemWorkers = taskSystem->GetWorkerCount();
            const uint32_t availableWorkers = taskSystemWorkers > 1 ? taskSystemWorkers - 1 : taskSystemWorkers;
            batch->WorkerLaneLimit = std::min({ m_MaxWorkerLanes, std::max(availableWorkers, 1u),
                                                static_cast<uint32_t>(batch->WorkerItemIndices.size()) });
            batch->LaneTasks.reserve(batch->WorkerLaneLimit);
            const std::weak_ptr<BatchState> weakBatch = batch;
            for (uint32_t lane = 0; lane < batch->WorkerLaneLimit; lane++)
            {
                const String taskName = "Editor asset import lane " + std::to_string(lane);
                batch->LaneTasks.push_back(Task::Create(
                  taskName,
                  [weakBatch, lane]() {
                      const std::shared_ptr<BatchState> state = weakBatch.lock();
                      if (state != nullptr)
                          RunWorkerLane(*state, lane);
                  },
                  TaskPriority::Normal));
            }
        }

        TracyPlot("Editor import queued files", static_cast<int64_t>(batch->WorkerItemIndices.size()));
        {
            Lock lock(m_Mutex);
            m_Batch = batch;
            if (taskSystem != nullptr)
            {
                for (const Ref<Task>& laneTask : batch->LaneTasks)
                    taskSystem->Submit(laneTask);
            }
        }
    }

    void ImportScheduler::RunWorkerLane(BatchState& batch, uint32_t laneIndex)
    {
        ZoneScopedN("ImportScheduler::WorkerLane");
        ZoneValue(laneIndex);

        while (!batch.CancelRequested.load(std::memory_order_acquire))
        {
            const uint32_t workerItem = batch.NextWorkerItem.fetch_add(1, std::memory_order_acq_rel);
            if (workerItem >= batch.WorkerItemIndices.size())
                break;
            if (batch.CancelRequested.load(std::memory_order_acquire))
                break;

            const uint32_t sequence = batch.WorkerItemIndices[workerItem];
            BatchState::Item& item = batch.Items[sequence];
            {
                Lock lock(batch.Mutex);
                if (item.State != ScheduledImportState::PendingWorker)
                    continue;
                item.State = ScheduledImportState::RunningWorker;
            }

            Ref<Asset> asset;
            const String sourcePath = item.Result.Task.SourcePath.string();
            const auto queueTime = std::chrono::steady_clock::now() - item.QueuedAt;
            TracyPlot("Editor import queue time us",
                      static_cast<int64_t>(std::chrono::duration_cast<std::chrono::microseconds>(queueTime).count()));
            TracyPlot("Editor import queued files",
                      static_cast<int64_t>(batch.WorkerItemIndices.size() -
                                           std::min(workerItem + 1, (uint32_t)batch.WorkerItemIndices.size())));

            {
                ZoneScopedN("ImportScheduler::ImportFile");
                ZoneText(sourcePath.c_str(), sourcePath.size());
                if (item.SerializedImporterMutex != nullptr)
                {
                    Lock importerLock(*item.SerializedImporterMutex);
                    if (!batch.CancelRequested.load(std::memory_order_acquire))
                        asset = Importer::Get().ImportDeferred(item.Result.Task.SourcePath, item.Result.Task.Options);
                }
                else
                    asset = Importer::Get().ImportDeferred(item.Result.Task.SourcePath, item.Result.Task.Options);
            }

            Lock lock(batch.Mutex);
            if (item.State != ScheduledImportState::RunningWorker)
                continue;
            item.Result.Asset = std::move(asset);
            if (batch.CancelRequested.load(std::memory_order_acquire))
                item.Result.Status = ImportResultStatus::Canceled;
            else
                item.Result.Status = item.Result.Asset != nullptr ? ImportResultStatus::Succeeded : ImportResultStatus::Failed;
            item.State = ScheduledImportState::Ready;
            batch.Progress.CompletedFiles++;
        }
    }

    bool ImportScheduler::ProcessCompleted(const CompletionHandler& completionHandler, uint32_t maxPerFrame)
    {
        ZoneScopedN("ImportScheduler::PublishBatch");

        std::shared_ptr<BatchState> batch;
        {
            Lock lock(m_Mutex);
            batch = m_Batch;
        }
        if (batch == nullptr)
            return false;

        uint32_t processed = 0;
        while (processed < maxPerFrame)
        {
            ImportTask mainThreadTask;
            ImportResult completedResult;
            bool runOnMainThread = false;
            bool publish = false;
            {
                Lock lock(batch->Mutex);
                if (batch->NextPublishSequence >= batch->Items.size())
                    break;

                BatchState::Item& item = batch->Items[batch->NextPublishSequence];
                if (item.State == ScheduledImportState::PendingMainThread)
                {
                    item.State = ScheduledImportState::RunningMainThread;
                    mainThreadTask = item.Result.Task;
                    runOnMainThread = true;
                }
                else if (item.State == ScheduledImportState::Ready)
                {
                    completedResult = item.Result;
                    item.Result.Asset = nullptr;
                    item.State = ScheduledImportState::Published;
                    batch->NextPublishSequence++;
                    publish = true;
                }
                else
                    break;
            }

            if (runOnMainThread)
            {
                const String sourcePath = mainThreadTask.SourcePath.string();
                const BatchState::Item& queuedItem = batch->Items[mainThreadTask.Sequence];
                const auto queueTime = std::chrono::steady_clock::now() - queuedItem.QueuedAt;
                TracyPlot("Editor import queue time us",
                          static_cast<int64_t>(std::chrono::duration_cast<std::chrono::microseconds>(queueTime).count()));

                Ref<Asset> asset;
                {
                    ZoneScopedN("ImportScheduler::ImportFile");
                    ZoneText(sourcePath.c_str(), sourcePath.size());
                    asset = Importer::Get().Import(mainThreadTask.SourcePath, mainThreadTask.Options);
                }

                Lock lock(batch->Mutex);
                BatchState::Item& item = batch->Items[mainThreadTask.Sequence];
                if (item.State == ScheduledImportState::RunningMainThread)
                {
                    item.Result.Asset = std::move(asset);
                    item.Result.Status = item.Result.Asset != nullptr ? ImportResultStatus::Succeeded : ImportResultStatus::Failed;
                    item.State = ScheduledImportState::Ready;
                    batch->Progress.CompletedFiles++;
                }
                continue;
            }

            if (publish)
            {
                completionHandler(completedResult);
                processed++;
            }
        }

        Vector<Ref<Task>> laneTasks;
        bool allPublished = false;
        {
            Lock lock(batch->Mutex);
            allPublished = batch->NextPublishSequence == batch->Items.size();
            laneTasks = batch->LaneTasks;
        }

        const bool lanesComplete = std::all_of(laneTasks.begin(), laneTasks.end(), [](const Ref<Task>& task) { return task->IsComplete(); });
        if (!allPublished || !lanesComplete)
            return false;

        {
            Lock lock(batch->Mutex);
            batch->LaneTasks.clear();
            batch->WorkerItemIndices.clear();
            batch->Items.clear();
            batch->NextPublishSequence = 0;
            batch->Progress.Active = false;
            batch->Progress.CurrentAssetName.clear();
        }
        return true;
    }

    void ImportScheduler::Shutdown()
    {
        std::shared_ptr<BatchState> batch;
        {
            Lock lock(m_Mutex);
            batch = m_Batch;
            if (batch != nullptr)
                batch->CancelRequested.store(true, std::memory_order_release);
        }
        if (batch == nullptr)
            return;

        Vector<Ref<Task>> laneTasks;
        {
            Lock lock(batch->Mutex);
            laneTasks = batch->LaneTasks;
        }
        for (const Ref<Task>& laneTask : laneTasks)
            laneTask->Wait();

        {
            Lock lock(batch->Mutex);
            for (BatchState::Item& item : batch->Items)
            {
                if (item.State == ScheduledImportState::PendingWorker || item.State == ScheduledImportState::RunningWorker ||
                    item.State == ScheduledImportState::PendingMainThread || item.State == ScheduledImportState::RunningMainThread)
                {
                    item.Result.Asset = nullptr;
                    item.Result.Status = ImportResultStatus::Canceled;
                    item.State = ScheduledImportState::Published;
                    batch->Progress.CompletedFiles++;
                }
            }
            batch->LaneTasks.clear();
            batch->WorkerItemIndices.clear();
            batch->Items.clear();
            batch->Progress = {};
        }

        Lock lock(m_Mutex);
        if (m_Batch == batch)
            m_Batch.reset();
    }

    bool ImportScheduler::IsActive() const
    {
        std::shared_ptr<BatchState> batch;
        {
            Lock lock(m_Mutex);
            batch = m_Batch;
        }
        if (batch == nullptr)
            return false;
        Lock lock(batch->Mutex);
        return batch->Progress.Active;
    }

    ImportProgress ImportScheduler::GetProgress() const
    {
        std::shared_ptr<BatchState> batch;
        {
            Lock lock(m_Mutex);
            batch = m_Batch;
        }
        if (batch == nullptr)
            return {};

        Lock lock(batch->Mutex);
        ImportProgress progress = batch->Progress;
        if (progress.Active && batch->NextPublishSequence < batch->Items.size())
            progress.CurrentAssetName = batch->Items[batch->NextPublishSequence].Result.Task.SourcePath.filename().string();
        return progress;
    }

    uint32_t ImportScheduler::GetWorkerLaneLimit() const
    {
        std::shared_ptr<BatchState> batch;
        {
            Lock lock(m_Mutex);
            batch = m_Batch;
        }
        if (batch == nullptr)
            return 0;
        Lock lock(batch->Mutex);
        return batch->WorkerLaneLimit;
    }
} // namespace Crowny

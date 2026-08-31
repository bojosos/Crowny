#pragma once

#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/RenderAPI/Texture.h"

#include "Editor/ProjectLibrary.h"

namespace Crowny
{

    enum class AssetPreviewStatus
    {
        Queued,
        Loading,
        Ready,
        Failed
    };

    struct AssetPreviewResult
    {
        AssetPreviewStatus Status = AssetPreviewStatus::Queued;
        Ref<Texture> Image;
        String Details;
        String Error;
        float Duration = 0.0f;
        uint32_t Channels = 0;
        uint32_t SampleRate = 0;
    };

    struct AssetPreviewCacheStats
    {
        size_t Entries = 0;
        size_t ReservedBytes = 0;
        size_t Pending = 0;
        size_t Running = 0;
    };

    class AssetPreviewService
    {
    public:
        static constexpr size_t DEFAULT_CACHE_BUDGET_BYTES = 32u * 1024u * 1024u;

        explicit AssetPreviewService(size_t cacheBudgetBytes = DEFAULT_CACHE_BUDGET_BYTES) : m_CacheBudgetBytes(cacheBudgetBytes) {}
        ~AssetPreviewService();

        const AssetPreviewResult* Request(const FileEntry& entry, uint32_t size);
        void Update();
        void CancelPending();
        void Invalidate(const UUID& uuid);
        void Clear();
        AssetPreviewCacheStats GetStats() const;

        static bool Supports(AssetType type);

    private:
        struct WorkItem;

        static void ExecutePreviewWork(const Ref<WorkItem>& work);
        static void CancelPreviewWork(const Ref<WorkItem>& work);
        void StartPendingWork();
        void FinalizeCompletedWork();
        bool MakeRoomFor(size_t reservedBytes);
        void RemovePendingWork(const Ref<WorkItem>& work);
        void ReleaseReservation(const Ref<WorkItem>& work);
        void EraseCacheEntry(const UUID& uuid, bool cancel);
        bool IsRunning(const Ref<WorkItem>& work) const;

        static constexpr uint32_t MAX_PREVIEW_DIMENSION = 256;
        static constexpr size_t MAX_PENDING = 128;
        static constexpr size_t MAX_CACHE_ENTRIES = 256;
        static constexpr size_t MAX_CONCURRENT = 2;

        UnorderedMap<UUID, Ref<WorkItem>> m_Cache;
        Deque<Ref<WorkItem>> m_Pending;
        Vector<Ref<WorkItem>> m_Running;
        uint64_t m_AccessTick = 0;
        size_t m_CacheBudgetBytes = DEFAULT_CACHE_BUDGET_BYTES;
        size_t m_ReservedBytes = 0;
        AssetPreviewResult m_QueueFullResult{ AssetPreviewStatus::Queued, nullptr, {}, "Preview queue is busy" };
        AssetPreviewResult m_BudgetFullResult{ AssetPreviewStatus::Queued, nullptr, {}, "Preview cache budget is busy" };
    };

} // namespace Crowny

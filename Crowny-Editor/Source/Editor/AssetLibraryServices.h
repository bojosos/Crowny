#pragma once

#include "Editor/AssetLibraryTypes.h"

#include <functional>
#include <memory>

namespace Crowny
{
    class AssetManifest;
    class SpecificImporter;
    class Task;

    class AssetIndex
    {
    public:
        const Ref<DirectoryEntry>& GetRoot() const { return m_Root; }
        Ref<DirectoryEntry>& GetRoot() { return m_Root; }
        void SetRoot(const Ref<DirectoryEntry>& root);
        void Clear();

        Ref<LibraryEntry> FindEntry(const Path& path) const;
        Ref<FileEntry> AddFile(DirectoryEntry* parent, const Path& path);
        Ref<DirectoryEntry> AddDirectory(DirectoryEntry* parent, const Path& path);
        void Remove(const Ref<LibraryEntry>& entry);
        Vector<Ref<LibraryEntry>> Search(const String& pattern, const Vector<AssetType>& assetTypes,
                                         const Ref<DirectoryEntry>& rootEntry = nullptr) const;

        void Register(const UUID& uuid, const Path& sourcePath);
        void Unregister(const UUID& uuid);
        Path UuidToPath(const UUID& uuid) const;
        UnorderedMap<UUID, Path>& GetUuidPaths() { return m_UuidToPath; }
        const UnorderedMap<UUID, Path>& GetUuidPaths() const { return m_UuidToPath; }

        AssetType GetAssetType(const Path& path) const;
        AssetType GetAssetType(const UUID& uuid) const;
        String GetAssetName(const UUID& uuid) const;

    private:
        Ref<DirectoryEntry> m_Root;
        UnorderedMap<UUID, Path> m_UuidToPath;
    };

    struct AssetFileSystemDiff
    {
        Path ScanRoot;
        Vector<Path> AddedDirectories;
        Vector<Path> AddedFiles;
        Vector<Ref<LibraryEntry>> RemovedEntries;
        Vector<Ref<FileEntry>> FilesToImport;
        Vector<Path> DanglingMetadata;

        bool Empty() const;
    };

    class AssetFileSystemScanner
    {
    public:
        AssetFileSystemDiff Scan(const Path& assetRoot, const Path& requestedPath, const AssetIndex& index) const;

        static Path ResolvePath(const Path& assetRoot, const Path& path);
        static bool IsPathWithin(const Path& root, const Path& candidate);
        static bool IsMetadata(const Path& path);
        static Path GetMetadataPath(const Path& path);
    };

    class AssetMetadataStore
    {
    public:
        bool Save(const Path& path, const Ref<AssetMetadata>& metadata, const Vector<Ref<AssetMetadata>>& dependents = {},
                  String* outError = nullptr) const;

        struct LoadResult;
        struct DependentReconciliation;

        LoadResult Load(const Path& path) const;
        DependentReconciliation ReconcileDependents(const Vector<Ref<AssetMetadata>>& existing, const Vector<Ref<Asset>>& importedAssets) const;

        void SaveIndex(const Path& path, const Ref<DirectoryEntry>& root) const;
        Ref<DirectoryEntry> LoadIndex(const Path& path) const;
    };

    enum class AssetMetadataLoadStatus
    {
        Missing,
        Loaded,
        // The primary was missing or corrupt and the last-good backup supplied the result.
        Recovered,
        // Neither copy passed schema and identity validation. No replacement UUID was generated.
        Corrupt
    };

    struct AssetMetadataStore::LoadResult
    {
        AssetMetadataLoadStatus Status = AssetMetadataLoadStatus::Missing;
        Ref<AssetMetadata> Metadata;
        Vector<Ref<AssetMetadata>> Dependents;
        uint32_t Version = 0;
        String Error;

        explicit operator bool() const { return Status == AssetMetadataLoadStatus::Loaded || Status == AssetMetadataLoadStatus::Recovered; }
    };

    using AssetMetadataLoadResult = AssetMetadataStore::LoadResult;

    struct AssetDependentAssignment
    {
        Ref<Asset> Asset;
        Ref<AssetMetadata> Metadata;
    };

    struct AssetMetadataStore::DependentReconciliation
    {
        Vector<AssetDependentAssignment> Assignments;
        Vector<Ref<AssetMetadata>> Orphans;
    };

    using AssetDependentReconciliation = AssetMetadataStore::DependentReconciliation;

    class AssetFilesystemOperations
    {
    public:
        bool Move(const Path& source, const Path& destination, bool overwrite) const;
        bool CopyFile(const Path& source, const Path& destination, bool overwrite) const;
        bool CopyFileAtomic(const Path& source, const Path& destination, bool overwrite, String* outError = nullptr) const;
        bool CopyDirectory(const Path& source, const Path& destination, bool overwrite) const;
        bool CreateDirectory(const Path& path) const;
        bool Remove(const Path& path) const;
    };

    class BuildManifestSelection
    {
    public:
        bool SetIncluded(const Ref<FileEntry>& entry, bool include, const AssetMetadataStore& metadataStore) const;
        Vector<Ref<FileEntry>> Collect(const Ref<DirectoryEntry>& root) const;
    };

    class ImportScheduler
    {
    public:
        using CompletionHandler = std::function<void(const ImportResult&)>;
        static constexpr uint32_t DEFAULT_MAX_WORKER_LANES = 2;

        explicit ImportScheduler(uint32_t maxWorkerLanes = DEFAULT_MAX_WORKER_LANES);
        ~ImportScheduler();

        void Schedule(Vector<ImportTask> tasks);
        bool ProcessCompleted(const CompletionHandler& completionHandler, uint32_t maxPerFrame = 4);
        void Shutdown();

        bool IsActive() const;
        ImportProgress GetProgress() const;
        uint32_t GetWorkerLaneLimit() const;

    private:
        struct BatchState;

        static void RunWorkerLane(BatchState& batch, uint32_t laneIndex);

        mutable Mutex m_Mutex;
        std::shared_ptr<BatchState> m_Batch;
        uint32_t m_MaxWorkerLanes;
    };
} // namespace Crowny

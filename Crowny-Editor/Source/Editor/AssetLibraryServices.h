#pragma once

#include "Editor/AssetLibraryTypes.h"

#include <functional>
#include <thread>

namespace Crowny
{
    class AssetManifest;

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
        void Save(const Path& path, const Ref<AssetMetadata>& metadata, const Vector<Ref<AssetMetadata>>& dependents = {}) const;
        Ref<AssetMetadata> Load(const Path& path, Vector<Ref<AssetMetadata>>* outDependents = nullptr) const;

        void SaveIndex(const Path& path, const Ref<DirectoryEntry>& root) const;
        Ref<DirectoryEntry> LoadIndex(const Path& path) const;
    };

    class AssetFilesystemOperations
    {
    public:
        bool Move(const Path& source, const Path& destination, bool overwrite) const;
        bool CopyFile(const Path& source, const Path& destination, bool overwrite) const;
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

        ~ImportScheduler();

        void Schedule(Vector<ImportTask> tasks);
        bool ProcessCompleted(const CompletionHandler& completionHandler, uint32_t maxPerFrame = 4);
        void Shutdown();

        bool IsActive() const { return m_Progress.Active.load(); }
        const ImportProgress& GetProgress() const { return m_Progress; }

    private:
        void ImportWorker(Vector<ImportTask> tasks);

        std::thread m_Worker;
        Mutex m_Mutex;
        Vector<ImportResult> m_Completed;
        ImportProgress m_Progress;
    };
} // namespace Crowny

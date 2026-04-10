#pragma once

#include "Crowny/Common/Module.h"

#include "Crowny/Assets/Asset.h"
#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Assets/AssetManifest.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Import/ImportOptions.h"
#include "Crowny/Serialization/CerealDataStreamArchive.h"
#include "Crowny/Utils/UUIDDirectory.h"

#include "Editor/Settings/ProjectSettings.h"

#include <atomic>
#include <thread>

namespace Crowny
{
    struct DirectoryEntry;
    struct FileEntry;

    enum class LibraryEntryType
    {
        File,
        Directory
    };

    struct LibraryEntry
    {
        LibraryEntry() = default;
        LibraryEntry(const Path& path, const String& name, DirectoryEntry* parent, LibraryEntryType type);
        virtual ~LibraryEntry() = default;

        LibraryEntryType Type = LibraryEntryType::File;
        Path Filepath;
        String ElementName;
        size_t ElementNameHash = 0; // Bug: Since I use ToLower on the name before calculating the hash, on Unix
                                    // platforms files with same names will both get highlighted.

        std::time_t LastUpdateTime = 0; // TODO: Consider removing this as it is a bit hard to keep track of
        DirectoryEntry* Parent = nullptr;
    };

    struct FileEntry : public LibraryEntry
    {
        FileEntry() = default;
        FileEntry(const Path& path, const String& name, DirectoryEntry* parent);
        ~FileEntry() = default;

        Ref<AssetMetadata> Metadata;                        // Primary asset metadata
        Vector<Ref<AssetMetadata>> DependentMetadata;       // Sub-assets (materials, textures from mesh import)
        uint32_t Filesize = 0;
    };

    struct DirectoryEntry : public LibraryEntry
    {
        DirectoryEntry() = default;
        DirectoryEntry(const Path& path, const String& name, DirectoryEntry* parent);
        ~DirectoryEntry() = default;

        Vector<Ref<LibraryEntry>> Children;
    };

    struct ImportTask
    {
        FileEntry* Entry = nullptr;
        Ref<ImportOptions> Options;
        bool ForceReimport = false;
    };

    struct ImportResult
    {
        ImportTask Task;
        Ref<Asset> Asset;
    };

    struct ImportProgress
    {
        std::atomic<bool> Active{false};
        std::atomic<uint32_t> TotalFiles{0};
        std::atomic<uint32_t> CompletedFiles{0};
        String CurrentAssetName;

        float GetFraction() const
        {
            uint32_t total = TotalFiles.load();
            return total > 0 ? (float)CompletedFiles.load() / (float)total : 0.0f;
        }
    };

    class ProjectLibrary : public Module<ProjectLibrary>
    {
    public:
        ProjectLibrary();
        ~ProjectLibrary();

        void Refresh(const Path& path);
        void RefreshAsync(const Path& path);
        void ProcessCompletedImports();
        bool IsImporting() const { return m_ImportProgress.Active.load(); }
        const ImportProgress& GetImportProgress() const { return m_ImportProgress; }
        const Ref<DirectoryEntry>& GetRoot() const { return m_RootEntry; }
        Ref<LibraryEntry> FindEntry(const Path& path) const;

        Vector<Ref<LibraryEntry>> Search(const String& pattern, const Vector<AssetType>& assetTypes = {},
                                         const Ref<DirectoryEntry>& rootEntry = nullptr);

        void MoveEntry(const Path& oldPath, const Path& newPath, bool overwrite = false);
        void CopyEntry(const Path& oldPath, const Path& newPath, bool overwrite = false);
        void CreateFolderEntry(const Path& path);
        void CreateEntry(const Ref<Asset>& asset, const Path& path);
        void DeleteEntry(const Path& path);
        void Reimport(const Path& path, const Ref<ImportOptions>& importOptions = nullptr, bool forceReimport = false);

        Ref<AssetMetadata> FindAssetMetadata(const Path& path) const;
        Path UuidToPath(const UUID& uuid) const;

        void SaveEntry(const Ref<Asset>& asset);
        void SetIncludeInBuild(const Path& path, bool force);
        Vector<Ref<FileEntry>> GetAssetsForBuild() const;
        AssetHandle<Asset> Load(const Path& path);
        AssetHandle<Asset> Load(const FileEntry* entry);
        const Path& GetAssetFolder() const { return m_AssetFolder; }

        static const Path ASSET_DIR;
        static const Path INTERNAL_ASSET_DIR;

        void LoadLibrary();
        void UnloadLibrary();
        void SaveLibrary();

        AssetType GetAssetType(const Path& path) const;
        AssetType GetAssetType(const UUID& uuid) const;

        Vector<UUID> GetAllAssets(AssetType type) const;

    private:
        void SerializeMetadata(const Path& path, const Ref<AssetMetadata>& metadata, const Vector<Ref<AssetMetadata>>& dependents = {});
        Ref<AssetMetadata> DeserializeMetadata(const Path& path, Vector<Ref<AssetMetadata>>* outDependents = nullptr);

        void SerializeLibraryEntries(const Path& path);
        Ref<DirectoryEntry> DeserializeLibraryEntries(const Path& libEntriesPath);

        bool IsUpToDate(FileEntry* entry) const;
        Ref<FileEntry> AddAssetInternal(DirectoryEntry* entry, const Path& path, const Ref<ImportOptions>& importOptions = nullptr,
                                        bool forceReimport = false);
        Ref<DirectoryEntry> AddDirectoryInternal(DirectoryEntry* parent, const Path& path);
        void DeleteAssetInternal(Ref<FileEntry> asset);
        void DeleteDirectoryInternal(Ref<DirectoryEntry> directory);
        bool ReimportAssetInternal(FileEntry* entry, const Ref<ImportOptions>& importOptions = nullptr, bool forceReimport = false);
        void CreateInternalParentHierarchy(const Path& fullPath, DirectoryEntry** newHierarchyRoot, DirectoryEntry** newHierarchyLeaf);

        Path GetMetadataPath(const Path& path) const;
        bool IsMetadata(const Path& path) const;

        void MakeEntriesRelative();
        void MakeEntriesAbsolute();
        void ClearEntries();

        static const char* LIBRARY_ENTRIES_FILENAME;
        static const char* ASSET_MANIFEST_FILENAME;

    private:
        bool m_IsLoaded;
        Ref<AssetManifest> m_AssetManifest;
        Path m_AssetFolder;
        Path m_ProjectFolder;
        Ref<DirectoryEntry> m_RootEntry;
        Ref<ProjectSettings> m_ProjectSettings;
        UUIDDirectory m_UuidDirectory;

        UnorderedMap<UUID, Path> m_UuidToPath;

        // Async import state
        std::thread m_ImportThread;
        Mutex m_ImportMutex;
        Vector<ImportResult> m_CompletedImports;
        ImportProgress m_ImportProgress;
        bool m_RefreshPending = false;
        Path m_PendingRefreshPath;

        void RefreshScan(const Path& path);
        void ImportWorker(Vector<ImportTask> tasks);
        void FinalizeImport(const ImportResult& result);
    };

} // namespace Crowny
#pragma once

#include "Crowny/Common/Module.h"

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Assets/AssetManifest.h"
#include "Crowny/Utils/UUIDDirectory.h"

#include "Editor/AssetLibraryServices.h"
#include "Editor/Settings/ProjectSettings.h"

namespace Crowny
{
    class ProjectLibrary : public Module<ProjectLibrary>
    {
    public:
        ProjectLibrary();
        ~ProjectLibrary();

        void Refresh(const Path& path);
        void RefreshAsync(const Path& path);
        void ProcessCompletedImports();
        bool IsImporting() const { return m_ImportScheduler.IsActive(); }
        ImportProgress GetImportProgress() const { return m_ImportScheduler.GetProgress(); }
        const Ref<DirectoryEntry>& GetRoot() const { return m_AssetIndex.GetRoot(); }
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
        bool TryGetAssetId(const Path& sourcePath, AssetType expectedType, UUID& outUuid) const;
        bool TryGetSourcePath(const UUID& uuid, AssetType expectedType, Path& outSourcePath) const;

        bool SaveEntry(const Ref<Asset>& asset, const Path& path);
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
        String GetAssetName(const UUID& uuid) const;

    private:
        bool IsUpToDate(FileEntry* entry) const;
        Ref<FileEntry> AddAssetInternal(DirectoryEntry* entry, const Path& path, const Ref<ImportOptions>& importOptions = nullptr,
                                        bool forceReimport = false);
        Ref<DirectoryEntry> AddDirectoryInternal(DirectoryEntry* parent, const Path& path);
        void DeleteAssetInternal(Ref<FileEntry> asset);
        void DeleteDirectoryInternal(Ref<DirectoryEntry> directory);
        bool EnsureMetadataLoaded(FileEntry* entry);
        bool CommitImportedAssets(FileEntry* entry, const Vector<Ref<Asset>>& assets, const Ref<ImportOptions>& importOptions);
        bool ReimportAssetInternal(FileEntry* entry, const Ref<ImportOptions>& importOptions = nullptr, bool forceReimport = false);
        void CreateInternalParentHierarchy(const Path& fullPath, DirectoryEntry** newHierarchyRoot, DirectoryEntry** newHierarchyLeaf);

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
        Ref<ProjectSettings> m_ProjectSettings;
        UUIDDirectory m_UuidDirectory;
        AssetIndex m_AssetIndex;
        AssetFileSystemScanner m_Scanner;
        AssetMetadataStore m_MetadataStore;
        AssetFilesystemOperations m_Filesystem;
        BuildManifestSelection m_BuildSelection;
        ImportScheduler m_ImportScheduler;

        bool m_RefreshPending = false;
        Path m_PendingRefreshPath;

        Vector<ImportTask> ApplyFilesystemDiff(const AssetFileSystemDiff& diff, bool importSynchronously);
        void FinalizeImport(const ImportResult& result);
    };

} // namespace Crowny

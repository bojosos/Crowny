#include "cwepch.h"

#include "Editor/ProjectLibrary.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Serialization/MaterialSerializer.h"
#include "Crowny/Serialization/NodeGraphSerializer.h"

#include "Editor/Editor.h"
#include "Editor/EditorUtils.h"

namespace Crowny
{
    const Path TEMP_DIR = "Temp";
    const Path INTERNAL_TEMP_DIR = PROJECT_INTERNAL_DIR / TEMP_DIR;

    const Path ProjectLibrary::ASSET_DIR = "Assets";
    const Path ProjectLibrary::INTERNAL_ASSET_DIR = PROJECT_INTERNAL_DIR / ASSET_DIR;
    const char* ProjectLibrary::ASSET_MANIFEST_FILENAME = "AssetManifest.yaml";
    const char* ProjectLibrary::LIBRARY_ENTRIES_FILENAME = "Entries.asset";

    LibraryEntry::LibraryEntry(const Path& path, const String& name, DirectoryEntry* parent, LibraryEntryType type)
      : Filepath(path), ElementName(name), Parent(parent), Type(type)
    {
        String lower = name;
        StringUtils::ToLower(lower);
        ElementNameHash = Hash(lower);
    }

    FileEntry::FileEntry(const Path& path, const String& name, DirectoryEntry* parent)
      : LibraryEntry(path, name, parent, LibraryEntryType::File), Filesize(0)
    {
    }

    DirectoryEntry::DirectoryEntry(const Path& path, const String& name, DirectoryEntry* parent)
      : LibraryEntry(path, name, parent, LibraryEntryType::Directory)
    {
    }

    ProjectLibrary::ProjectLibrary() : m_IsLoaded(false) {}

    ProjectLibrary::~ProjectLibrary()
    {
        m_ImportScheduler.Shutdown();
        ClearEntries();
    }

    void ProjectLibrary::Refresh(const Path& path)
    {
        if (m_AssetIndex.GetRoot() == nullptr)
            m_AssetIndex.SetRoot(CreateRef<DirectoryEntry>(m_AssetFolder, m_AssetFolder.filename().string(), nullptr));

        const AssetFileSystemDiff diff = m_Scanner.Scan(m_AssetFolder, path, m_AssetIndex);
        ApplyFilesystemDiff(diff, true);
    }

    Vector<ImportTask> ProjectLibrary::ApplyFilesystemDiff(const AssetFileSystemDiff& diff, bool importSynchronously)
    {
        for (const Path& metadataPath : diff.DanglingMetadata)
        {
            CW_ENGINE_WARN("Removing dangling metadata '{}'.", metadataPath);
            m_Filesystem.Remove(metadataPath);
        }

        for (const Ref<LibraryEntry>& entry : diff.RemovedEntries)
        {
            if (entry->Type == LibraryEntryType::Directory)
                DeleteDirectoryInternal(StaticRefCast<DirectoryEntry>(entry));
            else
                DeleteAssetInternal(StaticRefCast<FileEntry>(entry));
        }

        for (const Path& directoryPath : diff.AddedDirectories)
        {
            Ref<LibraryEntry> parentEntry = FindEntry(directoryPath.parent_path());
            DirectoryEntry* parent = nullptr;
            if (parentEntry == nullptr)
                CreateInternalParentHierarchy(directoryPath, nullptr, &parent);
            else if (parentEntry->Type == LibraryEntryType::Directory)
                parent = static_cast<DirectoryEntry*>(parentEntry.get());

            if (parent != nullptr && FindEntry(directoryPath) == nullptr)
                AddDirectoryInternal(parent, directoryPath);
        }

        Vector<ImportTask> tasks;
        tasks.reserve(diff.FilesToImport.size() + diff.AddedFiles.size());
        for (const Ref<FileEntry>& entry : diff.FilesToImport)
            tasks.push_back({ entry, nullptr, false, false });

        for (const Path& filePath : diff.AddedFiles)
        {
            Ref<LibraryEntry> parentEntry = FindEntry(filePath.parent_path());
            DirectoryEntry* parent = nullptr;
            if (parentEntry == nullptr)
                CreateInternalParentHierarchy(filePath, nullptr, &parent);
            else if (parentEntry->Type == LibraryEntryType::Directory)
                parent = static_cast<DirectoryEntry*>(parentEntry.get());

            if (parent == nullptr)
                continue;
            tasks.push_back({ m_AssetIndex.AddFile(parent, filePath), nullptr, false, false });
        }

        if (importSynchronously)
        {
            for (const ImportTask& task : tasks)
                ReimportAssetInternal(task.Entry.get(), task.Options, task.ForceReimport);
            tasks.clear();
        }
        return tasks;
    }

    void ProjectLibrary::RefreshAsync(const Path& path)
    {
        if (m_ImportScheduler.IsActive())
        {
            m_RefreshPending = true;
            m_PendingRefreshPath = path;
            return;
        }

        if (m_AssetIndex.GetRoot() == nullptr)
            m_AssetIndex.SetRoot(CreateRef<DirectoryEntry>(m_AssetFolder, m_AssetFolder.filename().string(), nullptr));

        const AssetFileSystemDiff diff = m_Scanner.Scan(m_AssetFolder, path, m_AssetIndex);
        Vector<ImportTask> tasks = ApplyFilesystemDiff(diff, false);
        if (tasks.empty())
            SaveLibrary();
        else
            m_ImportScheduler.Schedule(std::move(tasks));
    }

    void ProjectLibrary::ProcessCompletedImports()
    {
        if (!m_ImportScheduler.IsActive())
            return;

        const bool completed = m_ImportScheduler.ProcessCompleted([this](const ImportResult& result) {
            try
            {
                FinalizeImport(result);
            }
            catch (const std::exception& error)
            {
                CW_ENGINE_ERROR("Failed to finalize import '{}': {}", result.Task.Entry != nullptr ? result.Task.Entry->Filepath : Path(),
                                error.what());
            }
        });

        if (!completed)
            return;

        SaveLibrary();
        if (m_RefreshPending)
        {
            m_RefreshPending = false;
            const Path pendingPath = m_PendingRefreshPath;
            m_PendingRefreshPath.clear();
            RefreshAsync(pendingPath);
        }
    }

    void ProjectLibrary::FinalizeImport(const ImportResult& result)
    {
        const Ref<FileEntry>& entryRef = result.Task.Entry;
        FileEntry* entry = entryRef.get();
        Ref<Asset> asset = result.Asset;

        if (entry == nullptr || FindEntry(entry->Filepath) != entryRef)
            return;

        entry->Revision++;

        if (!asset)
        {
            CW_ENGINE_WARN("Failed to import: {0}", entry->Filepath);
            return;
        }

        if (!result.Task.RunOnMainThread)
            asset->Init();

        entry->Filesize = fs::exists(entry->Filepath) ? (uint32_t)fs::file_size(entry->Filepath) : 0;
        if (entry->Metadata == nullptr)
            entry->Metadata = CreateRef<AssetMetadata>();
        entry->Metadata->Type = asset->GetAssetType();
        if (result.Task.Options)
            entry->Metadata->ImportOptions = result.Task.Options;
        if (!entry->Metadata->ImportOptions)
            entry->Metadata->ImportOptions = Importer::Get().CreateImportOptions(entry->Filepath);
        entry->LastUpdateTime = std::time(nullptr);

        auto& uuid = entry->Metadata->Uuid;
        if (uuid.Empty())
            uuid = UuidGenerator::Generate();

        const Path metaPath = AssetFileSystemScanner::GetMetadataPath(entry->Filepath);
        m_MetadataStore.Save(metaPath, entry->Metadata, entry->DependentMetadata);

        Path outputPath = m_UuidDirectory.GetPath(uuid);
        outputPath.replace_filename(outputPath.filename().string() + ".asset");
        m_AssetManifest->RegisterAsset(uuid, outputPath);
        m_AssetIndex.Register(uuid, entry->Filepath);

        if (asset->GetAssetType() == AssetType::Scene || asset->GetAssetType() == AssetType::Prefab)
            m_Filesystem.CopyFile(entry->Filepath, outputPath, true);
        else
            AssetManager::TryGet()->Save(asset, outputPath);
    }

    void ProjectLibrary::ClearEntries() { m_AssetIndex.Clear(); }

    Ref<FileEntry> ProjectLibrary::AddAssetInternal(DirectoryEntry* parent, const Path& filepath, const Ref<ImportOptions>& importOptions,
                                                    bool forceReimport)
    {
        Ref<FileEntry> newAsset = m_AssetIndex.AddFile(parent, filepath);
        ReimportAssetInternal(newAsset.get(), importOptions, forceReimport);
        return newAsset;
    }

    Ref<DirectoryEntry> ProjectLibrary::AddDirectoryInternal(DirectoryEntry* parent, const Path& dirPath)
    {
        return m_AssetIndex.AddDirectory(parent, dirPath);
    }

    void ProjectLibrary::DeleteAssetInternal(Ref<FileEntry> asset)
    {
        auto unregisterUuid = [&](const UUID& uuid) {
            Path outPath;
            if (m_AssetManifest->UuidToFilepath(uuid, outPath))
            {
                if (fs::is_regular_file(outPath))
                    m_Filesystem.Remove(outPath);
                m_AssetManifest->UnregisterAsset(uuid);
            }
            m_AssetIndex.Unregister(uuid);
        };

        if (asset->Metadata != nullptr)
            unregisterUuid(asset->Metadata->Uuid);

        for (const auto& dep : asset->DependentMetadata)
            unregisterUuid(dep->Uuid);

        m_AssetIndex.Remove(asset);
    }

    void ProjectLibrary::DeleteDirectoryInternal(Ref<DirectoryEntry> directory)
    {
        Vector<Ref<LibraryEntry>> childrenToDestroy = directory->Children;
        for (const auto& child : childrenToDestroy)
        {
            if (child->Type == LibraryEntryType::Directory)
                DeleteDirectoryInternal(StaticRefCast<DirectoryEntry>(child));
            else
                DeleteAssetInternal(StaticRefCast<FileEntry>(child));
        }

        m_AssetIndex.Remove(directory);
        directory->Children.clear();
    }

    bool ProjectLibrary::ReimportAssetInternal(FileEntry* entry, const Ref<ImportOptions>& importOptions, bool forceReimport)
    {
        Path metaPath = entry->Filepath;
        metaPath = metaPath.replace_filename(metaPath.filename().string() + ".meta");
        if (entry->Metadata == nullptr)
        {
            if (fs::is_regular_file(metaPath))
            {
                Vector<Ref<AssetMetadata>> dependents;
                Ref<AssetMetadata> loadedMeta = m_MetadataStore.Load(metaPath, &dependents);
                if (loadedMeta != nullptr)
                {
                    entry->Metadata = loadedMeta;
                    entry->DependentMetadata = dependents;
                    m_AssetIndex.Register(loadedMeta->Uuid, entry->Filepath);
                    for (const auto& dep : dependents)
                        m_AssetIndex.Register(dep->Uuid, entry->Filepath);
                }
            }
        }

        if (!IsUpToDate(entry) || forceReimport)
        {
            entry->Revision++;
            Ref<ImportOptions> curImportOptions = nullptr;
            if (importOptions == nullptr)
            {
                if (entry->Metadata != nullptr)
                    curImportOptions = entry->Metadata->ImportOptions;
                else
                    curImportOptions = Importer::Get().CreateImportOptions(entry->Filepath);
            }
            else
                curImportOptions = importOptions;

            Vector<Ref<Asset>> assets = Importer::Get().ImportAll(entry->Filepath, curImportOptions);
            entry->Filesize = fs::exists(entry->Filepath) ? (uint32_t)fs::file_size(entry->Filepath) : 0;
            if (assets.empty())
                return false;

            // Primary asset (first in the list)
            Ref<Asset> primaryAsset = assets[0];
            if (!primaryAsset)
                return false;
            if (entry->Metadata == nullptr)
                entry->Metadata = CreateRef<AssetMetadata>();
            entry->Metadata->Type = primaryAsset->GetAssetType();

            entry->Metadata->ImportOptions = curImportOptions;
            entry->LastUpdateTime = std::time(nullptr);
            if (entry->Metadata->Uuid.Empty())
                entry->Metadata->Uuid = UuidGenerator::Generate();

            // Save primary asset
            auto saveAsset = [&](const Ref<Asset>& asset, const UUID& uuid) {
                Path outputPath = m_UuidDirectory.GetPath(uuid);
                outputPath.replace_filename(outputPath.filename().string() + ".asset");
                m_AssetManifest->RegisterAsset(uuid, outputPath);
                m_AssetIndex.Register(uuid, entry->Filepath);

                if (asset->GetAssetType() == AssetType::Scene || asset->GetAssetType() == AssetType::Prefab)
                    m_Filesystem.CopyFile(entry->Filepath, outputPath, true);
                else
                    AssetManager::TryGet()->Save(asset, outputPath);
            };

            saveAsset(primaryAsset, entry->Metadata->Uuid);

            // Clear old dependents
            for (const auto& depMeta : entry->DependentMetadata)
            {
                Path outPath;
                if (m_AssetManifest->UuidToFilepath(depMeta->Uuid, outPath))
                {
                    if (fs::is_regular_file(outPath))
                        m_Filesystem.Remove(outPath);
                    m_AssetManifest->UnregisterAsset(depMeta->Uuid);
                }
                m_AssetIndex.Unregister(depMeta->Uuid);
            }
            entry->DependentMetadata.clear();

            // Save dependent assets (materials, textures, etc.)
            for (uint32_t i = 1; i < (uint32_t)assets.size(); i++)
            {
                Ref<Asset>& depAsset = assets[i];
                if (!depAsset)
                    continue;

                Ref<AssetMetadata> depMeta = CreateRef<AssetMetadata>();
                depMeta->Type = depAsset->GetAssetType();
                depMeta->Uuid = UuidGenerator::Generate();
                depMeta->IncludeInBuild = true;

                saveAsset(depAsset, depMeta->Uuid);
                entry->DependentMetadata.push_back(depMeta);
            }

            const Path metaPath = AssetFileSystemScanner::GetMetadataPath(entry->Filepath);
            m_MetadataStore.Save(metaPath, entry->Metadata, entry->DependentMetadata);
            return true;
        }
        return false;
    }

    Vector<Ref<LibraryEntry>> ProjectLibrary::Search(const String& pattern, const Vector<AssetType>& assetTypes, const Ref<DirectoryEntry>& rootEntry)
    {
        return m_AssetIndex.Search(pattern, assetTypes, rootEntry);
    }

    void ProjectLibrary::MoveEntry(const Path& oldPath, const Path& newPath, bool overwrite)
    {
        const Path oldFullPath = AssetFileSystemScanner::ResolvePath(m_AssetFolder, oldPath);
        const Path newFullPath = AssetFileSystemScanner::ResolvePath(m_AssetFolder, newPath);
        if (!AssetFileSystemScanner::IsPathWithin(m_AssetFolder, oldFullPath) || oldFullPath == m_AssetFolder || !fs::exists(oldFullPath))
            return;

        const Path parentPath = newFullPath.parent_path();
        if (!fs::is_directory(parentPath))
        {
            CW_ENGINE_WARN("File move failed. Destination '{}' does not exist.", parentPath);
            return;
        }

        Ref<LibraryEntry> oldEntry = FindEntry(oldFullPath);
        if (fs::exists(newFullPath))
        {
            if (!overwrite)
            {
                CW_ENGINE_WARN("File move failed. Destination '{}' already exists.", newFullPath);
                return;
            }
            if (AssetFileSystemScanner::IsPathWithin(m_AssetFolder, newFullPath))
            {
                Ref<LibraryEntry> destinationEntry = FindEntry(newFullPath);
                if (destinationEntry != nullptr)
                {
                    if (destinationEntry->Type == LibraryEntryType::File)
                        DeleteAssetInternal(StaticRefCast<FileEntry>(destinationEntry));
                    else
                        DeleteDirectoryInternal(StaticRefCast<DirectoryEntry>(destinationEntry));
                }
            }
            m_Filesystem.Remove(newFullPath);
        }

        if (!m_Filesystem.Move(oldFullPath, newFullPath, false))
            return;

        const Path oldMetaPath = AssetFileSystemScanner::GetMetadataPath(oldFullPath);
        const Path newMetaPath = AssetFileSystemScanner::GetMetadataPath(newFullPath);
        if (fs::is_regular_file(oldMetaPath))
        {
            if (overwrite && fs::exists(newMetaPath))
                m_Filesystem.Remove(newMetaPath);
            m_Filesystem.Move(oldMetaPath, newMetaPath, false);
        }

        if (oldEntry == nullptr)
        {
            if (AssetFileSystemScanner::IsPathWithin(m_AssetFolder, newFullPath))
                Refresh(newFullPath);
            return;
        }

        if (!AssetFileSystemScanner::IsPathWithin(m_AssetFolder, newFullPath))
        {
            if (oldEntry->Type == LibraryEntryType::File)
                DeleteAssetInternal(StaticRefCast<FileEntry>(oldEntry));
            else
                DeleteDirectoryInternal(StaticRefCast<DirectoryEntry>(oldEntry));
            return;
        }

        DirectoryEntry* oldParent = oldEntry->Parent;
        if (oldParent != nullptr)
        {
            const auto iter = std::find(oldParent->Children.begin(), oldParent->Children.end(), oldEntry);
            if (iter != oldParent->Children.end())
                oldParent->Children.erase(iter);
        }

        DirectoryEntry* newEntryParent = nullptr;
        Ref<LibraryEntry> newEntryParentLibrary = FindEntry(parentPath);
        if (newEntryParentLibrary != nullptr && newEntryParentLibrary->Type == LibraryEntryType::Directory)
            newEntryParent = static_cast<DirectoryEntry*>(newEntryParentLibrary.get());
        if (newEntryParent == nullptr)
            CreateInternalParentHierarchy(newFullPath, nullptr, &newEntryParent);

        newEntryParent->Children.push_back(oldEntry);
        oldEntry->Parent = newEntryParent;
        oldEntry->Filepath = newFullPath;
        oldEntry->ElementName = newFullPath.filename().string();
        String lower = oldEntry->ElementName;
        StringUtils::ToLower(lower);
        oldEntry->ElementNameHash = Hash(lower);

        Stack<LibraryEntry*> entries;
        entries.push(oldEntry.get());
        while (!entries.empty())
        {
            LibraryEntry* current = entries.top();
            entries.pop();
            if (current->Type == LibraryEntryType::File)
            {
                FileEntry* file = static_cast<FileEntry*>(current);
                if (file->Metadata != nullptr)
                    m_AssetIndex.Register(file->Metadata->Uuid, file->Filepath);
                for (const Ref<AssetMetadata>& dependent : file->DependentMetadata)
                    m_AssetIndex.Register(dependent->Uuid, file->Filepath);
                continue;
            }

            DirectoryEntry* directory = static_cast<DirectoryEntry*>(current);
            for (const Ref<LibraryEntry>& child : directory->Children)
            {
                child->Filepath = (directory->Filepath / child->ElementName).lexically_normal();
                entries.push(child.get());
            }
        }
    }

    void ProjectLibrary::DeleteEntry(const Path& path)
    {
        const Path fullPath = AssetFileSystemScanner::ResolvePath(m_AssetFolder, path);
        if (!AssetFileSystemScanner::IsPathWithin(m_AssetFolder, fullPath) || fullPath == m_AssetFolder)
        {
            CW_ENGINE_WARN("Refusing to delete project-library path '{}'.", fullPath);
            return;
        }

        Ref<LibraryEntry> entry = FindEntry(fullPath);

        if (fs::exists(fullPath))
            m_Filesystem.Remove(fullPath);
        if (entry != nullptr && entry->Type == LibraryEntryType::File)
        {
            const Path metadataPath = AssetFileSystemScanner::GetMetadataPath(fullPath);
            if (fs::is_regular_file(metadataPath))
                m_Filesystem.Remove(metadataPath);
        }

        if (entry != nullptr)
        {
            if (entry->Type == LibraryEntryType::File)
                DeleteAssetInternal(StaticRefCast<FileEntry>(entry));
            else if (entry->Type == LibraryEntryType::Directory)
                DeleteDirectoryInternal(StaticRefCast<DirectoryEntry>(entry));
        }
    }

    void ProjectLibrary::SetIncludeInBuild(const Path& path, bool include)
    {
        const Ref<LibraryEntry> entry = FindEntry(path);
        if (entry != nullptr && entry->Type == LibraryEntryType::File)
            m_BuildSelection.SetIncluded(StaticRefCast<FileEntry>(entry), include, m_MetadataStore);
    }

    Path ProjectLibrary::UuidToPath(const UUID& uuid) const { return m_AssetIndex.UuidToPath(uuid); }

    Ref<AssetMetadata> ProjectLibrary::FindAssetMetadata(const Path& path) const
    {
        LibraryEntry* entry = FindEntry(path).get();
        if (entry == nullptr || entry->Type == LibraryEntryType::Directory)
            return nullptr;
        FileEntry* fileEntry = static_cast<FileEntry*>(entry);
        if (fileEntry->Metadata == nullptr)
            return nullptr;

        return fileEntry->Metadata;
    }

    Vector<UUID> ProjectLibrary::GetAllAssets(AssetType type) const
    {
        Vector<UUID> result;
        for (const auto& [uuid, path] : m_AssetIndex.GetUuidPaths())
        {
            if (m_AssetManifest->UuidExists(uuid) && m_AssetIndex.GetAssetType(path) == type)
                result.push_back(uuid);
        }
        return result;
    }

    String ProjectLibrary::GetAssetName(const UUID& uuid) const { return m_AssetIndex.GetAssetName(uuid); }

    AssetType ProjectLibrary::GetAssetType(const Path& path) const { return m_AssetIndex.GetAssetType(path); }

    AssetType ProjectLibrary::GetAssetType(const UUID& uuid) const { return m_AssetIndex.GetAssetType(uuid); }

    AssetHandle<Asset> ProjectLibrary::Load(const Path& path)
    {
        Ref<AssetMetadata> meta = FindAssetMetadata(path);
        if (meta == nullptr)
            return AssetHandle<Asset>();

        const UUID& uuid = meta->Uuid;
        return AssetManager::TryGet()->LoadFromUUID(uuid, true, true);
    }

    AssetHandle<Asset> ProjectLibrary::Load(const FileEntry* entry)
    {
        const Ref<AssetMetadata>& meta = entry->Metadata;
        if (meta == nullptr)
            return AssetHandle<Asset>();

        const UUID& uuid = meta->Uuid;
        return AssetManager::TryGet()->LoadFromUUID(uuid, true, true);
    }

    Vector<Ref<FileEntry>> ProjectLibrary::GetAssetsForBuild() const { return m_BuildSelection.Collect(m_AssetIndex.GetRoot()); }

    void ProjectLibrary::CopyEntry(const Path& oldPath, const Path& newPath, bool overwrite)
    {
        const Path oldFullPath = AssetFileSystemScanner::ResolvePath(m_AssetFolder, oldPath);
        const Path newFullPath = AssetFileSystemScanner::ResolvePath(m_AssetFolder, newPath);
        if (!AssetFileSystemScanner::IsPathWithin(m_AssetFolder, oldFullPath) || !fs::exists(oldFullPath) || oldFullPath == newFullPath)
            return;

        if (fs::exists(newFullPath))
        {
            if (!overwrite)
            {
                CW_ENGINE_WARN("File copy failed. Destination '{}' already exists.", newFullPath);
                return;
            }
            if (AssetFileSystemScanner::IsPathWithin(m_AssetFolder, newFullPath))
                DeleteEntry(newFullPath);
            else
                m_Filesystem.Remove(newFullPath);
        }

        const bool copied = fs::is_directory(oldFullPath) ? m_Filesystem.CopyDirectory(oldFullPath, newFullPath, false)
                                                          : m_Filesystem.CopyFile(oldFullPath, newFullPath, false);
        if (!copied)
            return;

        if (!AssetFileSystemScanner::IsPathWithin(m_AssetFolder, newFullPath))
            return;

        Path parentPath = newFullPath.parent_path();
        DirectoryEntry* newEntryParent = nullptr;
        LibraryEntry* newEntryParentLib = FindEntry(parentPath).get();
        if (newEntryParentLib != nullptr && newEntryParentLib->Type == LibraryEntryType::Directory)
        {
            newEntryParent = static_cast<DirectoryEntry*>(newEntryParentLib);
        }
        if (newEntryParent == nullptr)
            CreateInternalParentHierarchy(newFullPath, nullptr, &newEntryParent);

        LibraryEntry* oldEntry = FindEntry(oldFullPath).get();
        if (oldEntry == nullptr)
        {
            Refresh(newFullPath);
            return;
        }

        if (fs::is_regular_file(newFullPath))
        {
            CW_ENGINE_ASSERT(oldEntry->Type == LibraryEntryType::File);
            FileEntry* oldAssetEntry = static_cast<FileEntry*>(oldEntry);

            Ref<ImportOptions> importOptions;
            if (oldAssetEntry->Metadata != nullptr)
                importOptions = oldAssetEntry->Metadata->ImportOptions;

            AddAssetInternal(newEntryParent, newFullPath, importOptions, true);
        }
        else
        {
            CW_ENGINE_ASSERT(oldEntry->Type == LibraryEntryType::Directory);
            DirectoryEntry* oldDirEntry = static_cast<DirectoryEntry*>(oldEntry);

            DirectoryEntry* newDirEntry = AddDirectoryInternal(newEntryParent, newFullPath).get();
            Stack<Pair<DirectoryEntry*, DirectoryEntry*>> todos;
            todos.push({ oldDirEntry, newDirEntry });

            while (!todos.empty())
            {
                auto current = todos.top();
                todos.pop();

                DirectoryEntry* sourceDir = current.first;
                DirectoryEntry* dstDir = current.second;

                for (const auto& child : sourceDir->Children)
                {
                    Path childDstPath = dstDir->Filepath;
                    childDstPath /= child->Filepath.filename();

                    if (child->Type == LibraryEntryType::File)
                    {
                        FileEntry* childAssetEntry = static_cast<FileEntry*>(child.get());

                        Ref<ImportOptions> importOptions;
                        if (childAssetEntry->Metadata != nullptr)
                            importOptions = childAssetEntry->Metadata->ImportOptions;

                        AddAssetInternal(dstDir, childDstPath, importOptions, true);
                    }
                    else
                    {
                        DirectoryEntry* childSourceDirEntry = static_cast<DirectoryEntry*>(child.get());
                        DirectoryEntry* childDstDirEntry = AddDirectoryInternal(dstDir, childDstPath).get();
                        todos.push(std::make_pair(childSourceDirEntry, childDstDirEntry));
                    }
                }
            }
        }
    }

    void ProjectLibrary::CreateFolderEntry(const Path& path)
    {
        const Path fullPath = AssetFileSystemScanner::ResolvePath(m_AssetFolder, path);
        if (!AssetFileSystemScanner::IsPathWithin(m_AssetFolder, fullPath))
            return;

        if (fs::is_directory(fullPath))
            return;

        if (!m_Filesystem.CreateDirectory(fullPath))
            return;
        Path parentPath = fullPath.parent_path();

        DirectoryEntry* newEntryParent = nullptr;
        Ref<LibraryEntry> newEntryParentLib = FindEntry(parentPath);
        if (newEntryParentLib != nullptr)
        {
            CW_ENGINE_ASSERT(newEntryParentLib->Type == LibraryEntryType::Directory);
            newEntryParent = static_cast<DirectoryEntry*>(newEntryParentLib.get());
        }

        DirectoryEntry* newHierarchyParent = nullptr;
        if (newEntryParent == nullptr)
            CreateInternalParentHierarchy(fullPath, &newHierarchyParent, &newEntryParent);

        AddDirectoryInternal(newEntryParent, fullPath);
    }

    void ProjectLibrary::CreateEntry(const Ref<Asset>& asset, const Path& path)
    {
        if (asset == nullptr)
            return;

        const Path absPath = AssetFileSystemScanner::ResolvePath(m_AssetFolder, path);

        if (!AssetFileSystemScanner::IsPathWithin(m_AssetFolder, absPath))
        {
            CW_ENGINE_WARN("Attempted to create entry outside of asset folder: {0}", absPath);
            return;
        }

        DeleteEntry(absPath);
        asset->SetName(path.filename().replace_extension("").string());

        if (asset->GetAssetType() == AssetType::NodeGraph)
        {
            auto graph = StaticRefCast<NodeGraphAsset>(asset)->GetGraph();
            NodeGraphSerializer serializer(graph);
            serializer.Serialize(absPath);
        }
        else if (asset->GetAssetType() == AssetType::Material)
        {
            auto material = StaticRefCast<Material>(asset);
            MaterialSerializer serializer(material);
            serializer.Serialize(absPath);
        }
        else
        {
            AssetManager::TryGet()->Save(asset, absPath);
        }

        Path parentDirPath = absPath.parent_path();
        Ref<LibraryEntry> parentEntry = FindEntry(parentDirPath);

        DirectoryEntry* entryParent = nullptr;
        if (parentEntry == nullptr)
            CreateInternalParentHierarchy(absPath, nullptr, &entryParent);
        else
            entryParent = static_cast<DirectoryEntry*>(parentEntry.get());
        AddAssetInternal(entryParent, absPath, nullptr, true);
    }

    void ProjectLibrary::Reimport(const Path& path, const Ref<ImportOptions>& importOptions, bool forceReimport)
    {
        LibraryEntry* entry = FindEntry(path).get();
        if (entry != nullptr)
        {
            if (entry->Type == LibraryEntryType::File)
            {
                FileEntry* assetEntry = static_cast<FileEntry*>(entry);
                ReimportAssetInternal(assetEntry, importOptions, forceReimport);
            }
        }
    }

    Ref<LibraryEntry> ProjectLibrary::FindEntry(const Path& path) const { return m_AssetIndex.FindEntry(path); }

    void ProjectLibrary::CreateInternalParentHierarchy(const Path& path, DirectoryEntry** newHierarchyRoot, DirectoryEntry** newHierarchyLeaf)
    {
        Path parentPath = path;
        DirectoryEntry* newEntryParent = nullptr;
        Stack<Path> parentPaths;
        do
        {
            Path newParentPath = parentPath.parent_path();
            if (newParentPath == parentPath)
                break;

            LibraryEntry* newEntryParentLib = FindEntry(newParentPath).get();
            if (newEntryParentLib != nullptr)
            {
                CW_ENGINE_ASSERT(newEntryParentLib->Type == LibraryEntryType::Directory);
                newEntryParent = static_cast<DirectoryEntry*>(newEntryParentLib);
                break;
            }

            parentPaths.push(newParentPath);
            parentPath = newParentPath;
        } while (true);

        CW_ENGINE_ASSERT(newEntryParent != nullptr);
        if (newHierarchyRoot != nullptr)
            *newHierarchyRoot = newEntryParent;

        while (!parentPaths.empty())
        {
            Path curPath = parentPaths.top();
            parentPaths.pop();
            newEntryParent = AddDirectoryInternal(newEntryParent, curPath).get();
        }

        if (newHierarchyLeaf != nullptr)
            *newHierarchyLeaf = newEntryParent;
    }

    bool ProjectLibrary::IsUpToDate(FileEntry* entry) const
    {
        if (entry->Metadata == nullptr)
            return false;
        Path internalPath;
        if (!m_AssetManifest->UuidToFilepath(entry->Metadata->Uuid, internalPath))
            return false;
        if (!fs::exists(internalPath))
            return false;

        std::time_t lastModifiedTime = EditorUtils::FileTimeToCTime(fs::last_write_time(entry->Filepath));
        std::time_t lastUpdateTime = entry->LastUpdateTime;
        return lastModifiedTime <= lastUpdateTime;
    }

    void ProjectLibrary::MakeEntriesRelative()
    {
        std::function<void(LibraryEntry*, const Path&)> makeRelative = [&](LibraryEntry* entry, const Path& root) {
            entry->Filepath = fs::relative(entry->Filepath, root);
            if (entry->Type == LibraryEntryType::Directory)
            {
                DirectoryEntry* dirEntry = static_cast<DirectoryEntry*>(entry);
                for (const auto& child : dirEntry->Children)
                    makeRelative(child.get(), root);
            }
        };

        if (m_AssetIndex.GetRoot() != nullptr)
            makeRelative(m_AssetIndex.GetRoot().get(), m_AssetFolder);
    }

    void ProjectLibrary::MakeEntriesAbsolute()
    {
        std::function<void(LibraryEntry*)> makeAbsolute = [&](LibraryEntry* entry) {
            entry->Filepath = (m_AssetFolder / entry->Filepath).lexically_normal();
            if (entry->Type == LibraryEntryType::Directory)
            {
                DirectoryEntry* dirEntry = static_cast<DirectoryEntry*>(entry);
                for (const auto& child : dirEntry->Children)
                    makeAbsolute(child.get());
            }
        };

        if (m_AssetIndex.GetRoot() != nullptr)
            makeAbsolute(m_AssetIndex.GetRoot().get());
    }

    void ProjectLibrary::LoadLibrary()
    {
        UnloadLibrary();

        m_ProjectFolder = Editor::Get().GetProjectPath();
        m_AssetFolder = m_ProjectFolder / ASSET_DIR;

        m_AssetIndex.SetRoot(CreateRef<DirectoryEntry>(m_AssetFolder, m_AssetFolder.filename().string(), nullptr));
        const Path internalAssetPath = m_ProjectFolder / INTERNAL_ASSET_DIR;
        m_UuidDirectory = UUIDDirectory(internalAssetPath);

        Application::TryGet()->SetInternalDirectory(internalAssetPath);

        Path libEntriesPath = m_ProjectFolder / PROJECT_INTERNAL_DIR / LIBRARY_ENTRIES_FILENAME;

        if (fs::exists(libEntriesPath))
        {
            const Ref<DirectoryEntry> loadedRoot = m_MetadataStore.LoadIndex(libEntriesPath);
            if (loadedRoot != nullptr)
            {
                m_AssetIndex.SetRoot(loadedRoot);
                m_AssetIndex.GetRoot()->Parent = nullptr;
            }
        }

        String tabs;
        std::function<void(const Ref<LibraryEntry>&)> traverse = [&](const Ref<LibraryEntry>& entry) {
            CW_ENGINE_INFO("{0} Entry: {1}, {2}", tabs, entry->Filepath, entry->ElementName);
            if (entry->Type == LibraryEntryType::Directory)
            {
                tabs += "\t";
                for (const auto& child : StaticRefCast<DirectoryEntry>(entry)->Children)
                    traverse(child);
                tabs = tabs.substr(0, tabs.size() - 2);
            }
        };
        // CW_ENGINE_INFO("Original entries");
        // traverse(m_RootEntry);

        MakeEntriesAbsolute();
        Path assetManifestPath = m_ProjectFolder / PROJECT_INTERNAL_DIR / ASSET_MANIFEST_FILENAME;
        if (fs::exists(assetManifestPath))
            m_AssetManifest = AssetManifest::Deserialize(assetManifestPath, m_ProjectFolder);
        if (m_AssetManifest == nullptr)
            m_AssetManifest = CreateRef<AssetManifest>("ProjectLibrary");

        AssetManager::TryGet()->RegisterAssetManifest(m_AssetManifest);

        Stack<DirectoryEntry*> todos; // Load meta files
        todos.push(m_AssetIndex.GetRoot().get());
        Vector<Ref<LibraryEntry>> deletedEntries;
        while (!todos.empty())
        {
            DirectoryEntry* curDir = todos.top();
            todos.pop();

            for (const auto& child : curDir->Children)
            {
                if (child->Type == LibraryEntryType::File)
                {
                    Ref<FileEntry> entry = StaticRefCast<FileEntry>(child);
                    if (fs::is_regular_file(entry->Filepath))
                    {
                        if (entry->Metadata == nullptr)
                        {
                            Path metaPath = entry->Filepath;
                            metaPath = metaPath.replace_filename(metaPath.filename().string() + ".meta");
                            if (fs::is_regular_file(metaPath))
                                entry->Metadata = m_MetadataStore.Load(metaPath, &entry->DependentMetadata);
                        }

                        if (entry->Metadata != nullptr)
                        {
                            m_AssetIndex.Register(entry->Metadata->Uuid, entry->Filepath);
                            for (const Ref<AssetMetadata>& dependent : entry->DependentMetadata)
                                m_AssetIndex.Register(dependent->Uuid, entry->Filepath);
                        }
                    }
                    else
                        deletedEntries.push_back(entry);
                }
                else if (child->Type == LibraryEntryType::Directory)
                {
                    if (fs::is_directory(child->Filepath))
                        todos.push(static_cast<DirectoryEntry*>(child.get()));
                    else
                        deletedEntries.push_back(child);
                }
            }
        }

        for (const auto& deletedEntry : deletedEntries)
        {
            if (deletedEntry->Type == LibraryEntryType::File)
                DeleteAssetInternal(StaticRefCast<FileEntry>(deletedEntry));
            else
                DeleteDirectoryInternal(StaticRefCast<DirectoryEntry>(deletedEntry));
        }

        Path internalAssetFolder = m_ProjectFolder / INTERNAL_ASSET_DIR;
        if (fs::exists(internalAssetFolder))
        {
            Vector<Path> toDelete;
            auto processFile = [&](const Path& path) {
                UUID uuid = UUID(path.filename().replace_extension("").string());
                if (!uuid.Empty() && m_AssetIndex.GetUuidPaths().find(uuid) == m_AssetIndex.GetUuidPaths().end())
                {
                    m_AssetManifest->UnregisterAsset(uuid);
                    toDelete.push_back(path);
                }
            };

            for (const auto& fileIterator : fs::recursive_directory_iterator(internalAssetFolder))
            {
                if (fileIterator.is_regular_file())
                    processFile(fileIterator.path());
            }

            for (const auto& entry : toDelete)
                m_Filesystem.Remove(entry);
        }

        m_IsLoaded = true;
    }

    void ProjectLibrary::UnloadLibrary()
    {
        if (!m_IsLoaded)
            return;
        m_ImportScheduler.Shutdown();
        m_RefreshPending = false;
        m_PendingRefreshPath.clear();
        m_AssetFolder = Path();
        m_ProjectFolder = Path();
        ClearEntries();
        m_AssetIndex.SetRoot(CreateRef<DirectoryEntry>(m_AssetFolder, m_AssetFolder.filename().string(), nullptr));
        AssetManager::TryGet()->UnregisterAssetManifest(m_AssetManifest);
        m_AssetManifest = nullptr;
        m_IsLoaded = false;
    }

    void ProjectLibrary::SaveLibrary()
    {
        if (!m_IsLoaded)
            return;
        String tabs;
        std::function<void(const Ref<LibraryEntry>&)> traverse = [&](const Ref<LibraryEntry>& entry) {
            CW_ENGINE_INFO("{0} Entry: {1}, {2}", tabs, entry->Filepath, entry->ElementName);
            if (entry->Type == LibraryEntryType::Directory)
            {
                tabs += "\t";
                for (const auto& child : StaticRefCast<DirectoryEntry>(entry)->Children)
                    traverse(child);
                tabs = tabs.substr(0, tabs.size() - 2);
            }
        };
        // CW_ENGINE_INFO("Original entries");
        // traverse(m_RootEntry);

        MakeEntriesRelative();
        Path libEntriesPath = m_ProjectFolder / PROJECT_INTERNAL_DIR / LIBRARY_ENTRIES_FILENAME;
        try
        {
            m_MetadataStore.SaveIndex(libEntriesPath, m_AssetIndex.GetRoot());
        }
        catch (const std::exception& error)
        {
            CW_ENGINE_ERROR("Failed to save project-library entries '{}': {}", libEntriesPath, error.what());
        }
        MakeEntriesAbsolute();
        Path assetManifestPath = m_ProjectFolder / PROJECT_INTERNAL_DIR / ASSET_MANIFEST_FILENAME;
        AssetManifest::Serialize(m_AssetManifest, assetManifestPath, m_ProjectFolder);
    }

} // namespace Crowny

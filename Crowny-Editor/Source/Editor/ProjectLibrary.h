#pragma once

#include "Crowny/Common/Module.h"

#include "Crowny/Assets/Asset.h"
#include "Crowny/Assets/AssetManifest.h"
#include "Crowny/Import/ImportOptions.h"

#include "Editor/Settings/ProjectSettings.h"

#include "Crowny/Assets/CerealDataStreamArchive.h"

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

        LibraryEntryType Type;
        Path Filepath;
        String ElementName;
        size_t ElementNameHash = 0;

        std::time_t LastUpdateTime = 0;
        DirectoryEntry* Parent = nullptr;
    };

    struct FileEntry : public LibraryEntry
    {
        FileEntry() = default;
        FileEntry(const Path& path, const String& name, DirectoryEntry* parent);
        Ref<AssetMetadata> Metadata;
        uint32_t Filesize;

        template <class Archive> void Serialize(Archive& archive)
        {
            archive(Type, Filepath, ElementName, ElementNameHash, LastUpdateTime, Filesize);
        }
    };

    struct DirectoryEntry : public LibraryEntry
    {
        DirectoryEntry() = default;
        DirectoryEntry(const Path& path, const String& name, DirectoryEntry* parent);

        Vector<Ref<LibraryEntry>> Children;

        void Save(BinaryDataStreamOutputArchive& archive) const
        {
            archive(Type, Filepath, ElementName, ElementNameHash, LastUpdateTime, Children.size());
            for (auto& child : Children)
            {
                if (child->Type == LibraryEntryType::File)
                    archive(std::static_pointer_cast<FileEntry>(child));
                else
                    archive(std::static_pointer_cast<DirectoryEntry>(child));
            }
        }

        void Load(BinaryDataStreamOutputArchive& archive)
        {
            size_t numChildren = 0;
            archive(Type, Filepath, ElementName, ElementNameHash, LastUpdateTime, numChildren);

            for (size_t i = 0; i < numChildren; i++)
            {
                LibraryEntryType type = LibraryEntryType::File;
                archive(type);
                if (type == LibraryEntryType::File)
                {
                    Ref<FileEntry> childEntry = CreateRef<FileEntry>();
                    archive(childEntry);
                    Children.push_back(childEntry);
                    childEntry->Parent = this;
                }
                else
                {
                    Ref<DirectoryEntry> childEntry = CreateRef<DirectoryEntry>();
                    archive(childEntry);
                    Children.push_back(childEntry);
                    childEntry->Parent = this;
                }
            }
        }
    };

    class ProjectLibrary : public Module<ProjectLibrary>
    {
    public:
        ProjectLibrary();
        ~ProjectLibrary();

        void Refresh(const Path& path);
        const Ref<DirectoryEntry>& GetRoot() const { return m_RootEntry; }
        Ref<LibraryEntry> FindEntry(const Path& path) const;

        Vector<Ref<LibraryEntry>> Search(const String& pattern);

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
        Ref<Asset> Load(const Path& path);
        const Path& GetAssetFolder() const { return m_AssetFolder; }

        static const Path ASSET_DIR;
        static const Path INTERNAL_ASSET_DIR;

        void LoadLibrary();
        void UnloadLibrary();
        void SaveLibrary();

    private:
        void SerializeMetadata(const Path& path, const Ref<AssetMetadata>& metadata);
        Ref<AssetMetadata> DeserializeMetadata(const Path& path);

        void SerializeLibraryEntries(const Path& path);
        Ref<DirectoryEntry> DeserializeLibraryEntries(const Path& libEntriesPath);

        bool IsUpToDate(FileEntry* entry) const;
        Ref<FileEntry> AddAssetInternal(DirectoryEntry* entry, const Path& path,
                                        const Ref<ImportOptions>& importOptions = nullptr, bool forceReimport = false);
        Ref<DirectoryEntry> AddDirectoryInternal(DirectoryEntry* parent, const Path& path);
        void DeleteAssetInternal(Ref<FileEntry> asset);
        void DeleteDirectoryInternal(Ref<DirectoryEntry> directory);
        bool ReimportAssetInternal(FileEntry* entry, const Ref<ImportOptions>& importOptions = nullptr,
                                   bool forceReimport = false);
        void CreateInternalParentHierarchy(const Path& fullPath, DirectoryEntry** newHierarchyRoot,
                                           DirectoryEntry** newHierarchyLeaf);

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

        UnorderedMap<UUID, Path> m_UuidToPath;
    };

} // namespace Crowny
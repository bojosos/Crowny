#pragma once

#include "Crowny/Assets/AssetManifest.h"
#include "Crowny/Import/ImportOptions.h"

#include "Editor/Settings/ProjectSettings.h"

namespace Crowny
{

    class ProjectLibrary : public Module<ProjectLibrary>
    {
    public:
        struct DirectoryEntry;
        struct FileEntry;

        enum class LibraryEntryType
        {
            File,
            Directory
        };

        struct LibraryEntry
        {
            LibraryEntry();
            LibraryEntry(const Path& path, const String& name, DirectoryEntry* parent, LibraryEntryType type);

            LibraryEntryType Type;
            Path Filepath;
            String ElementName;
            size_t ElementNameHash = 0;

            DirectoryEntry* Parent = nullptr;
        };

        struct FileEntry : public LibraryEntry
        {
            FileEntry() = default;
            FileEntry(const Path& path, const String& name, DirectoryEntry* parent);
            Ref<AssetMetaData> Metadata;
            std::time_t LastUpdateTime = 0;
        };

        struct DirectoryEntry : public LibraryEntry
        {
            DirectoryEntry() = default;
            DirectoryEntry(const Path& path, const String& name, DirectoryEntry* parent);

            Vector<Ref<LibraryEntry>> Children;
        };

    public:
        ProjectLibrary();
        ~ProjectLibrary();

        void Refresh();
        const Ref<LibraryEntry> GetRoot() const { return m_RootEntry; }
        Ref<LibraryEntry> FindEntry(const Path& path) const;

        Vector<Ref<LibraryEntry>> Search(const String& pattern);

        void MoveEntry(const Path& oldPath, const Path& newPath, bool overwrite = false);
        void CopyEntry(const Path& oldPath, const Path& newPath, bool overwrite = false);
        void CreateFolderEntry(const Path& path);
        void CreateEntry(const Path& path);
        void DeleteEntry(const Path& path);
        void Reimport(const Path& path, const Ref<ImportOptions>& importOptions = nullptr, bool forceReimport = false);

        void SaveLibrary();

    private:
        bool IsUpToDate(const Ref<FileEntry>& entry) const;
        void AddResource(DirectoryEntry* entry, const Path& path, const Ref<ImportOptions>& importOptions);
        void AddDirectory(DirectoryEntry* parent, const Path& path);

        Ref<AssetManifest> m_AssetManifest;
        Ref<LibraryEntry> m_RootEntry;
        Ref<ProjectSettings> m_ProjectSettings;
    };

} // namespace Crowny
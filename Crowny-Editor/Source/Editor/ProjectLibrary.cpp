#include "cwepch.h"

#include "Editor/ProjectLibrary.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/Importer.h"

#include "Editor/EditorUtils.h"

namespace Crowny
{

    void ProjectLibrary::MoveEntry(const Path& oldPath, const Path& newPath, bool overwrite)
    {
        if (!overwrite)
        {
            if (std::filesystem::exists(newPath))
            {
                // TODO: Get suitable name (eg. Material (1))
            }
        }
        std::filesystem::rename(oldPath, newPath);
    }

    void ProjectLibrary::CopyEntry(const Path& oldPath, const Path& newPath, bool overwrite)
    {
        std::filesystem::copy(oldPath, newPath,
                              overwrite ? std::filesystem::copy_options::overwrite_existing
                                        : std::filesystem::copy_options::none);
    }

    void ProjectLibrary::CreateFolderEntry(const Path& path) { std::filesystem::create_directory(path); }

    void ProjectLibrary::CreateEntry(const Path& path) {}

    void ProjectLibrary::AddResource(DirectoryEntry* parent, const Path& path, const Ref<ImportOptions>& importOptions)
    {
        Ref<FileEntry> newResource = CreateRef<FileEntry>(path, path.filename(), parent);

        parent->Children.push_back(newResource);
    }

    void ProjectLibrary::AddDirectory(DirectoryEntry* parent, const Path& path)
    {
        Ref<DirectoryEntry> newDirectory = CreateRef<DirectoryEntry>(path, path.stem(), parent);
        parent->Children.push_back(newDirectory);
    }

    void ProjectLibrary::Reimport(const Path& path, const Ref<ImportOptions>& importOptions, bool forceReimport)
    {
        Ref<LibraryEntry> entry = FindEntry(path);
        if (entry == nullptr || entry->Type != LibraryEntryType::File)
            return;
        Ref<FileEntry> fileEntry = std::static_pointer_cast<FileEntry>(entry);
        Path metaPath = path.parent_path() / path.filename() / ".meta";
        if (fileEntry->Metadata == nullptr)
        {
            if (std::filesystem::exists(metaPath))
            {
                Ref<AssetMetaData> metadata = DeserializeMetadata(metaPath);
                if (metadata != nullptr)
                    m_UuidToPath[metadata->GetUuid()] = fileEntry->Filepath;
            }
        }

        if (!IsUpToDate(fileEntry) || forceReimport)
        {
            Ref<ImportOptions> curImportOptions = nullptr;
            if (importOptions != nullptr)
            {
                if (fileEntry->Metadata != nullptr)
                    curImportOptions = fileEntry->Metadata->GetImportOptions();
                else
                    curImportOptions = Importer::Get().CreateImportOptions(fileEntry->Filepath);
            }
            else
                curImportOptions = importOptions;

            // Ref<Asset> asset = Importer::Get().Import(fileEntry->Filepath, curImportOptions, uuid);
            // AssetManager::Get().Save(asset, ); // TODO: Save to projectPath/Cache/Uuid.asset
        }
    }

    bool ProjectLibrary::IsUpToDate(const Ref<FileEntry>& resource) const
    {
        if (resource->Metadata == nullptr)
            return false;
        Path internalPath;
        if (!m_AssetManifest->UuidFromFilepath(resource->Metadata->GetUuid(), internalPath))
            return false;

        if (!std::filesystem::exists(internalPath))
            return false;

        std::time_t lastModifiedTime =
          EditorUtils::FileTimeToCTime(std::filesystem::last_write_time(resource->Filepath));
        return false;
    }

    void ProjectLibrary::SaveLibrary()
    {
        // TODO: Save the assets
        // TODO: Save the manifest
    }

} // namespace Crowny
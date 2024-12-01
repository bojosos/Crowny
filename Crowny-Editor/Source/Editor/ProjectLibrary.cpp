#include "cwepch.h"

#include "Editor/ProjectLibrary.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Serialization/FileEncoder.h"
#include "Crowny/Serialization/ImportOptionsSerializer.h"

#include "Editor/Editor.h"
#include "Editor/EditorUtils.h"

#include "Crowny/Common/Yaml.h"

#include <regex>

CEREAL_REGISTER_TYPE(DirectoryEntry);
CEREAL_REGISTER_TYPE(FileEntry);
CEREAL_REGISTER_POLYMORPHIC_RELATION(LibraryEntry, DirectoryEntry)
CEREAL_REGISTER_POLYMORPHIC_RELATION(LibraryEntry, FileEntry)

namespace Crowny
{
    template <class Archive> void Save(Archive& archive, const LibraryEntry& entry)
    {
        archive(entry.Type, entry.Filepath, entry.ElementName, entry.LastUpdateTime);
    }

    template <class Archive> void Load(Archive& archive, LibraryEntry& entry)
    {
        archive(entry.Type, entry.Filepath, entry.ElementName, entry.LastUpdateTime);
    }

    void Load(BinaryDataStreamInputArchive& archive, FileEntry& entry)
    {
        archive(cereal::base_class<LibraryEntry>(&entry));
        String pathCopy = entry.ElementName;
        StringUtils::ToLower(pathCopy);
        entry.ElementNameHash = Hash(pathCopy);
        archive(entry.Filesize);
    }

    void Save(BinaryDataStreamOutputArchive& archive, const FileEntry& entry)
    {
        archive(cereal::base_class<LibraryEntry>(&entry), entry.Filesize);
    }

    void Save(BinaryDataStreamOutputArchive& archive, const DirectoryEntry& entry)
    {
        archive(cereal::base_class<LibraryEntry>(&entry), entry.Children);
    }

    void Load(BinaryDataStreamInputArchive& archive, DirectoryEntry& entry)
    {
        archive(cereal::base_class<LibraryEntry>(&entry), entry.Children);
        String pathCopy = entry.ElementName;
        StringUtils::ToLower(pathCopy);
        entry.ElementNameHash = Hash(pathCopy);
        for (auto& child : entry.Children)
            child->Parent = &entry;
    }

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

    ProjectLibrary::ProjectLibrary() : m_RootEntry(nullptr), m_IsLoaded(false) {}

    ProjectLibrary::~ProjectLibrary() { ClearEntries(); }

    void ProjectLibrary::Refresh(const Path& path)
    {

        if (std::search(path.begin(), path.end(), m_AssetFolder.begin(), m_AssetFolder.end()) == path.end())
            return;

        if (m_RootEntry == nullptr)
            m_RootEntry = CreateRef<DirectoryEntry>(m_AssetFolder, m_AssetFolder.filename().string(), nullptr);

        Path pathToSearch = path;
        Ref<LibraryEntry> entry = FindEntry(pathToSearch);
        if (entry == nullptr)
        {
            if (fs::exists(pathToSearch))
            {
                if (IsMetadata(pathToSearch))
                {
                    Path sourceFilePath = pathToSearch;
                    sourceFilePath = sourceFilePath.replace_extension("");

                    if (!fs::is_regular_file(sourceFilePath))
                    {
                        CW_ENGINE_WARN("Found a dangling metadata file. Deleting.");
                        fs::remove(pathToSearch);
                    }
                }
                else
                {
                    Path parentDirPath = pathToSearch.parent_path();
                    entry = FindEntry(parentDirPath);

                    DirectoryEntry* entryParent = nullptr;
                    DirectoryEntry* newHierarchyParent = nullptr;
                    if (entry == nullptr)
                        CreateInternalParentHierarchy(pathToSearch, &newHierarchyParent, &entryParent);
                    else
                        entryParent = static_cast<DirectoryEntry*>(entry.get());

                    if (fs::is_regular_file(pathToSearch))
                        AddAssetInternal(entryParent, pathToSearch);
                    else if (fs::is_directory(pathToSearch))
                        AddDirectoryInternal(entryParent, pathToSearch);
                }
            }
        }
        else if (entry->Type == LibraryEntryType::File)
        {
            if (fs::is_regular_file(entry->Filepath))
            {
                FileEntry* resEntry = static_cast<FileEntry*>(entry.get());
                ReimportAssetInternal(resEntry);
            }
            else
                DeleteAssetInternal(std::static_pointer_cast<FileEntry>(entry));
        }
        else if (entry->Type == LibraryEntryType::Directory)
        {
            if (!fs::is_directory(entry->Filepath))
                DeleteDirectoryInternal(std::static_pointer_cast<DirectoryEntry>(entry));
            else
            {
                Stack<DirectoryEntry*> todos;
                todos.push(static_cast<DirectoryEntry*>(entry.get()));

                Vector<bool> existingEntries;
                Vector<Ref<LibraryEntry>> toDelete;

                while (!todos.empty())
                {
                    DirectoryEntry* currentDir = todos.top();
                    todos.pop();
                    existingEntries.clear();
                    existingEntries.resize(currentDir->Children.size());
                    for (uint32_t i = 0; i < currentDir->Children.size(); i++)
                        existingEntries[i] = false;
                    for (auto& dirEntry : fs::directory_iterator(currentDir->Filepath))
                    {
                        if (dirEntry.is_regular_file())
                        {
                            Path filepath = dirEntry.path();
                            if (IsMetadata(filepath))
                            {
                                Path sourceFilepath = filepath;
                                sourceFilepath = sourceFilepath.replace_extension("");
                                if (!fs::is_regular_file(sourceFilepath))
                                {
                                    CW_ENGINE_ERROR("Found a danglind metadata file. Deleting.");
                                    fs::remove(filepath);
                                }
                            }
                            else
                            {
                                FileEntry* existingEntry = nullptr;
                                uint32_t idx = 0;
                                for (auto& child : currentDir->Children)
                                {
                                    if (child->Type == LibraryEntryType::File && child->Filepath == filepath)
                                    {
                                        existingEntries[idx] = true;
                                        existingEntry = static_cast<FileEntry*>(child.get());
                                        break;
                                    }

                                    idx++;
                                }

                                if (existingEntry != nullptr)
                                    ReimportAssetInternal(existingEntry);
                                else
                                    AddAssetInternal(currentDir, filepath);
                            }
                        }
                        else if (dirEntry.is_directory())
                        {
                            Path dirPath = dirEntry.path();
                            DirectoryEntry* existingEntry = nullptr;
                            uint32_t idx = 0;
                            for (auto& child : currentDir->Children)
                            {
                                if (child->Type == LibraryEntryType::Directory && child->Filepath == dirPath)
                                {
                                    existingEntries[idx] = true;
                                    existingEntry = static_cast<DirectoryEntry*>(child.get());
                                    break;
                                }
                                idx++;
                            }

                            if (existingEntry == nullptr)
                                AddDirectoryInternal(currentDir, dirPath);
                        }
                    }

                    for (uint32_t i = 0; i < existingEntries.size(); i++)
                    {
                        if (existingEntries[i])
                            continue;

                        toDelete.push_back(currentDir->Children[i]);
                    }

                    for (auto& child : toDelete)
                    {
                        if (child->Type == LibraryEntryType::Directory) // Here we get a crash on refresh
                            DeleteDirectoryInternal(std::static_pointer_cast<DirectoryEntry>(child));
                        else if (child->Type == LibraryEntryType::File)
                            DeleteAssetInternal(std::static_pointer_cast<FileEntry>(child));
                    }

                    toDelete.clear();

                    for (auto& child : currentDir->Children)
                    {
                        if (child->Type == LibraryEntryType::Directory)
                            todos.push(static_cast<DirectoryEntry*>(child.get()));
                    }
                }
            }
        }
    }

    void ProjectLibrary::ClearEntries() {}

    bool ProjectLibrary::IsMetadata(const Path& path) const { return path.extension() == ".meta"; }

    Path ProjectLibrary::GetMetadataPath(const Path& path) const
    {
        Path metaPath = path;
        metaPath = metaPath.replace_extension(metaPath.extension().string() + ".meta");
        return metaPath;
    }

    Ref<FileEntry> ProjectLibrary::AddAssetInternal(DirectoryEntry* parent, const Path& filepath,
                                                    const Ref<ImportOptions>& importOptions, bool forceReimport)
    {
        Ref<FileEntry> newAsset = CreateRef<FileEntry>(filepath, filepath.filename().string(), parent);
        parent->Children.push_back(newAsset);
        ReimportAssetInternal(newAsset.get(), importOptions, forceReimport);
        return newAsset;
    }

    Ref<DirectoryEntry> ProjectLibrary::AddDirectoryInternal(DirectoryEntry* parent, const Path& dirPath)
    {
        Ref<DirectoryEntry> newDir = CreateRef<DirectoryEntry>(dirPath, dirPath.filename().string(), parent);
        parent->Children.push_back(newDir);
        return newDir;
    }

    void ProjectLibrary::DeleteAssetInternal(Ref<FileEntry> asset)
    {
        if (asset->Metadata != nullptr)
        {
            auto& assetMetadata = asset->Metadata;
            const UUID& uuid = assetMetadata->Uuid;
            Path outPath;
            if (m_AssetManifest->UuidToFilepath(uuid, outPath))
            {
                if (fs::is_regular_file(outPath))
                    fs::remove(outPath);
                m_AssetManifest->UnregisterAsset(uuid);
            }
            m_UuidToPath.erase(uuid);
        }

        DirectoryEntry* parent = asset->Parent;
        auto iterFind = std::find_if(parent->Children.begin(), parent->Children.end(),
                                     [&](const Ref<LibraryEntry>& entry) { return entry == asset; });
        parent->Children.erase(iterFind);
        *asset = FileEntry();
    }

    void ProjectLibrary::SerializeMetadata(const Path& path, const Ref<AssetMetadata>& metadata)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Uuid" << YAML::Value << metadata->Uuid;
        out << YAML::Key << "IncludeInBuild" << YAML::Value << metadata->IncludeInBuild;
        out << YAML::Key << "TypeId" << YAML::Value << (uint32_t)metadata->Type;
        ImportOptionsSerializer::Serialize(out, metadata->ImportOptions);
        out << YAML::EndMap;
        if (!fs::is_directory(path.parent_path()))
            fs::create_directories(path.parent_path());
        FileSystem::WriteTextFile(path, out.c_str());
    }

    Ref<AssetMetadata> ProjectLibrary::DeserializeMetadata(const Path& path)
    {
        Ref<AssetMetadata> metadata = CreateRef<AssetMetadata>();
        String metadataText = FileSystem::OpenFile(path)->GetAsString();
        YAML::Node data = YAML::Load(metadataText);
        if (const auto& uuid = data["Uuid"])
            metadata->Uuid = uuid.as<UUID>();
        else
        {
            CW_ENGINE_WARN(
              "Metadata {0} does not have a uuid. Generating a random one. Correspnding asset may be broken.", path);
            metadata->Uuid = UuidGenerator::Generate();
        }

        if (const auto& includeInBuild = data["IncludeInBuild"])
            metadata->IncludeInBuild = includeInBuild.as<bool>();
        if (const auto& typeId = data["TypeId"])
            metadata->Type = (AssetType)typeId.as<uint32_t>();

        metadata->ImportOptions = ImportOptionsSerializer::Deserialize(data);
        return metadata;
    }

    void ProjectLibrary::SerializeLibraryEntries(const Path& libEntriesPath)
    {
        if (!fs::is_directory(libEntriesPath.parent_path()))
            fs::create_directories(libEntriesPath.parent_path());
        FileEncoder<DirectoryEntry, SerializerType::Binary> encoder(libEntriesPath);
        encoder.Encode(m_RootEntry);
    }

    Ref<DirectoryEntry> ProjectLibrary::DeserializeLibraryEntries(const Path& libEntriesPath)
    {
        FileDecoder<DirectoryEntry, SerializerType::Binary> decoder(libEntriesPath);
        return decoder.Decode();
    }

    void ProjectLibrary::DeleteDirectoryInternal(Ref<DirectoryEntry> directory)
    {
        if (directory == m_RootEntry)
            m_RootEntry = nullptr;
        Vector<Ref<LibraryEntry>> childrenToDestroy = directory->Children;
        for (auto& child : childrenToDestroy)
        {
            if (child->Type == LibraryEntryType::Directory)
                DeleteDirectoryInternal(std::static_pointer_cast<DirectoryEntry>(child));
            else
                DeleteAssetInternal(std::static_pointer_cast<FileEntry>(child));
        }

        DirectoryEntry* parent = directory->Parent;
        if (parent != nullptr)
        {
            auto iterFind = std::find_if(parent->Children.begin(), parent->Children.end(),
                                         [&](const Ref<LibraryEntry>& entry) { return entry == directory; });
            parent->Children.erase(iterFind);
        }
        *directory = DirectoryEntry();
    }

    bool ProjectLibrary::ReimportAssetInternal(FileEntry* entry, const Ref<ImportOptions>& importOptions,
                                               bool forceReimport)
    {
        Path metaPath = entry->Filepath;
        metaPath = metaPath.replace_filename(metaPath.filename().string() + ".meta");
        if (entry->Metadata == nullptr)
        {
            if (fs::is_regular_file(metaPath))
            {
                Ref<AssetMetadata> loadedMeta = DeserializeMetadata(metaPath);
                if (loadedMeta != nullptr)
                {
                    entry->Metadata = loadedMeta;
                    m_UuidToPath[loadedMeta->Uuid] = entry->Filepath;
                }
            }
        }

        if (!IsUpToDate(entry) || forceReimport)
        {
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

            Ref<Asset> asset = Importer::Get().Import(entry->Filepath, curImportOptions);
            entry->Filesize = (uint32_t)fs::file_size(entry->Filepath);
            if (asset == nullptr)
                return false;
            if (entry->Metadata == nullptr)
            {
                entry->Metadata = CreateRef<AssetMetadata>();
                entry->Metadata->Type =
                  asset->GetAssetType(); // Maybe do this every time so even if id gets messed up it works
            }

            entry->Metadata->ImportOptions = curImportOptions;
            entry->LastUpdateTime = std::time(nullptr);
            auto& uuid = entry->Metadata->Uuid;
            if (uuid.Empty())
                uuid = UuidGenerator::Generate();
            Path metaPath = GetMetadataPath(entry->Filepath);
            SerializeMetadata(metaPath, entry->Metadata);
            Path outputPath = m_UuidDirectory.GetPath(uuid);
            outputPath.replace_filename(outputPath.filename().string() + ".asset");
            m_AssetManifest->RegisterAsset(uuid, outputPath);
            AssetManager::Get().Save(asset, outputPath);
            return true;
        }
        return false;
    }

    Vector<Ref<LibraryEntry>> ProjectLibrary::Search(const String& pattern, const Vector<AssetType>& assetTypes,
                                                     const Ref<DirectoryEntry>& rootEntry)
    {
        Vector<Ref<LibraryEntry>> entries;
        const std::regex escape("[.^$|()\\[\\]{}*+?\\\\]");
        const String replace("\\\\&");
        const String escapedPattern = std::regex_replace(
          pattern, escape, replace, std::regex_constants::match_default | std::regex_constants::format_sed);

#ifdef WIN32
        const std::regex wildcard("\\\\\\*");
#else
        const std::regex wildcard("\\\\\\\\\\*");
#endif
        const String wildcardReplace(".*");
        const String searchPattern = std::regex_replace(escapedPattern, wildcard, ".*");

        const std::regex searchRegex(searchPattern, std::regex_constants::ECMAScript | std::regex_constants::icase);

        Stack<DirectoryEntry*> todos;
        if (rootEntry == nullptr)
            todos.push(m_RootEntry.get());
        else
            todos.push(rootEntry.get());
        while (!todos.empty())
        {
            DirectoryEntry* dirEntry = todos.top();
            todos.pop();

            for (const Ref<LibraryEntry>& child : dirEntry->Children)
            {
                if (std::regex_match(child->ElementName, searchRegex))
                {
                    if (assetTypes.empty())
                        entries.push_back(child);
                    else
                    {
                        if (child->Type == LibraryEntryType::File)
                        {
                            FileEntry* fileEntry = static_cast<FileEntry*>(child.get());
                            if (fileEntry->Metadata != nullptr)
                            {
                                const bool found = std::find(assetTypes.begin(), assetTypes.end(),
                                                             fileEntry->Metadata->Type) != assetTypes.end();
                                if (found)
                                    entries.push_back(child);
                            }
                        }
                    }
                }
                if (child->Type == LibraryEntryType::Directory)
                {
                    DirectoryEntry* directoryEntry = static_cast<DirectoryEntry*>(child.get());
                    todos.push(directoryEntry);
                }
            }
        }

        return entries;
    }

    void ProjectLibrary::MoveEntry(const Path& oldPath, const Path& newPath, bool overwrite)
    {
        Path oldFullPath = oldPath; // These don't work
        if (!oldFullPath.is_absolute())
            oldFullPath = fs::absolute(oldFullPath);

        Path newFullPath = newPath;
        if (!newFullPath.is_absolute())
            newFullPath = fs::absolute(newFullPath);

        Path parentPath = newFullPath.parent_path();
        if (!fs::is_directory(parentPath))
        {
            CW_ENGINE_WARN("File move failed. Destination {0} does not exist.", parentPath);
            return;
        }

        if (fs::is_regular_file(oldFullPath) || fs::is_directory(oldFullPath))
        {
            if (!overwrite)
            {
                // CW_ENGINE_INFO("Here: {0}, {1}", oldFullPath, newFullPath);
                if (!fs::exists(newFullPath))
                {
                    // CW_ENGINE_INFO("Here2");
                    fs::rename(oldFullPath, newFullPath);
                }
            }
            else
            {
                fs::rename(oldFullPath, newFullPath);
                // CW_ENGINE_INFO("Here2");
            }
            /*if (fs::exists(newFullPath))
            {
                if (overwrite)
                    fs::remove(newFullPath);
                else
                    CW_ENGINE_WARN("File {0} already exists.", newFullPath);
            }
            fs::rename(oldFullPath, newFullPath);*/
        }

        Path oldMetaPath = GetMetadataPath(oldFullPath);
        Path newMetaPath = GetMetadataPath(newFullPath);

        Ref<LibraryEntry> oldEntry = FindEntry(oldFullPath);
        if (oldEntry != nullptr)
        {
            CW_ENGINE_INFO("We have old entry");
            if (std::search(newFullPath.begin(), newFullPath.end(), m_AssetFolder.begin(), m_AssetFolder.end()) ==
                newFullPath.end())
            {
                CW_ENGINE_INFO("Search");
                if (oldEntry->Type == LibraryEntryType::File)
                    DeleteAssetInternal(std::static_pointer_cast<FileEntry>(oldEntry));
                else if (oldEntry->Type == LibraryEntryType::Directory)
                    DeleteDirectoryInternal(std::static_pointer_cast<DirectoryEntry>(oldEntry));
            }
            else
            {
                Ref<FileEntry> fileEntry = nullptr;
                if (oldEntry->Type == LibraryEntryType::File)
                {
                    fileEntry = std::static_pointer_cast<FileEntry>(oldEntry);
                    if (fileEntry->Metadata != nullptr)
                        m_UuidToPath[fileEntry->Metadata->Uuid] = newFullPath;
                }
                // CW_ENGINE_INFO("Old meta path");
                if (fs::is_regular_file(oldMetaPath))
                {
                    // CW_ENGINE_INFO("Rename");
                    fs::rename(oldMetaPath, newMetaPath);
                }

                DirectoryEntry* parent = oldEntry->Parent;
                auto iterFind = std::find(parent->Children.begin(), parent->Children.end(), oldEntry);
                if (iterFind != parent->Children.end())
                    parent->Children.erase(iterFind);

                Path parentPath = newFullPath.parent_path();
                DirectoryEntry* newEntryParent = nullptr;
                Ref<LibraryEntry> newEntryParentLib = FindEntry(parentPath);
                if (newEntryParentLib != nullptr)
                {
                    CW_ENGINE_ASSERT(newEntryParentLib->Type == LibraryEntryType::Directory);
                    newEntryParent = static_cast<DirectoryEntry*>(newEntryParentLib.get());
                }

                DirectoryEntry* newHierarchyParent = nullptr;
                if (newEntryParent == nullptr)
                    CreateInternalParentHierarchy(newFullPath, &newHierarchyParent, &newEntryParent);

                newEntryParent->Children.push_back(oldEntry);
                oldEntry->Parent = newEntryParent;
                oldEntry->Filepath = newFullPath;
                oldEntry->ElementName = newFullPath.filename().string();
                String lower = oldEntry->ElementName;
                StringUtils::ToLower(lower);
                oldEntry->ElementNameHash = Hash(lower);

                if (oldEntry->Type == LibraryEntryType::Directory)
                {
                    Stack<LibraryEntry*> todos;
                    todos.push(oldEntry.get());

                    while (!todos.empty())
                    {
                        LibraryEntry* curEntry = todos.top();
                        todos.pop();

                        DirectoryEntry* curDirEntry = static_cast<DirectoryEntry*>(curEntry);
                        for (auto& child : curDirEntry->Children)
                        {
                            child->Filepath = child->Parent->Filepath / child->ElementName;
                            if (child->Type == LibraryEntryType::Directory)
                                todos.push(child.get());
                        }
                    }
                }
            }
        }
        else
            Refresh(newFullPath);
    }

    void ProjectLibrary::DeleteEntry(const Path& path)
    {
        Path fullPath = path;
        if (!fullPath.is_absolute())
            fullPath = fs::absolute(fullPath);

        if (fs::exists(fullPath))
            fs::remove_all(fullPath);

        Ref<LibraryEntry> entry = FindEntry(fullPath);
        if (entry != nullptr)
        {
            if (entry->Type == LibraryEntryType::File)
                DeleteAssetInternal(std::static_pointer_cast<FileEntry>(entry));
            else if (entry->Type == LibraryEntryType::Directory)
                DeleteDirectoryInternal(std::static_pointer_cast<DirectoryEntry>(entry));
        }
    }

    void ProjectLibrary::SetIncludeInBuild(const Path& path, bool include)
    {
        LibraryEntry* entry = FindEntry(path).get();
        if (entry == nullptr || entry->Type == LibraryEntryType::Directory)
            return;

        FileEntry* fileEntry = static_cast<FileEntry*>(entry);
        if (fileEntry->Metadata == nullptr)
            return;

        bool save = fileEntry->Metadata->IncludeInBuild != include;
        fileEntry->Metadata->IncludeInBuild = include;

        if (save)
        {
            Path metaPath = fileEntry->Filepath;
            metaPath = metaPath.replace_filename(metaPath.filename().string() + ".meta");
            SerializeMetadata(metaPath, fileEntry->Metadata);
        }
    }

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
        for (auto [uuid, path] : m_UuidToPath)
        {
            if (m_AssetManifest->UuidExists(uuid) && GetAssetType(uuid) == type)
                result.push_back(uuid);
        }
        return result;
    }

    AssetType ProjectLibrary::GetAssetType(const Path& path) const
    {
        // yikes... called a lot
        LibraryEntry* entry = FindEntry(path).get();
        if (!entry)
            return AssetType::None;
        if (entry->Type == LibraryEntryType::File)
        {
            FileEntry* fileEntry = static_cast<FileEntry*>(entry);
            if (fileEntry->Metadata != nullptr)
                return fileEntry->Metadata->Type;
        }
        return AssetType::None;
    }

    AssetType ProjectLibrary::GetAssetType(const UUID& uuid) const
    {
        Path outPath;
        const auto& iter = m_UuidToPath.find(uuid);
        if (iter != m_UuidToPath.end())
            return GetAssetType(iter->second);
        return AssetType::None;
    }

    AssetHandle<Asset> ProjectLibrary::Load(const Path& path)
    {
        Ref<AssetMetadata> meta = FindAssetMetadata(path);
        if (meta == nullptr)
            return AssetHandle<Asset>();

        const UUID& uuid = meta->Uuid;
        return AssetManager::Get().LoadFromUUID(uuid, true, true);
    }

    AssetHandle<Asset> ProjectLibrary::Load(const FileEntry* entry)
    {
        const Ref<AssetMetadata>& meta = entry->Metadata;
        if (meta == nullptr)
            return AssetHandle<Asset>();

        const UUID& uuid = meta->Uuid;
        return AssetManager::Get().LoadFromUUID(uuid, true, true);
    }

    Vector<Ref<FileEntry>> ProjectLibrary::GetAssetsForBuild() const
    {
        Vector<Ref<FileEntry>> output;
        Stack<DirectoryEntry*> todos;
        todos.push(m_RootEntry.get());

        while (!todos.empty())
        {
            DirectoryEntry* current = todos.top();
            todos.pop();

            for (auto& child : current->Children)
            {
                if (child->Type == LibraryEntryType::File)
                {
                    FileEntry* assetEntry = static_cast<FileEntry*>(child.get());
                    if (assetEntry->Metadata != nullptr && assetEntry->Metadata->IncludeInBuild)
                        output.push_back(std::static_pointer_cast<FileEntry>(child));
                }
                else if (child->Type == LibraryEntryType::Directory)
                    todos.push(static_cast<DirectoryEntry*>(child.get()));
            }
        }

        return output;
    }

    void ProjectLibrary::CopyEntry(const Path& oldPath, const Path& newPath, bool overwrite)
    {
        Path oldFullPath = oldPath;
        if (!oldFullPath.is_absolute())
            oldFullPath = fs::absolute(oldFullPath);

        Path newFullPath = oldPath;
        if (!newFullPath.is_absolute())
            newFullPath = fs::absolute(newFullPath);

        if (!fs::exists(oldFullPath))
            return;

        fs::copy(oldPath, newPath, overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::none);

        if (std::search(newFullPath.begin(), newFullPath.end(), m_AssetFolder.begin(), m_AssetFolder.end()) ==
            newFullPath.end())
            return;

        Path parentPath = newFullPath.parent_path();
        DirectoryEntry* newEntryParent = nullptr;
        LibraryEntry* newEntryParentLib = FindEntry(parentPath).get();
        if (newEntryParentLib != nullptr)
        {
            CW_ENGINE_ASSERT(newEntryParentLib->Type == LibraryEntryType::Directory);
            newEntryParent = static_cast<DirectoryEntry*>(newEntryParentLib);
        }

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
            CW_ENGINE_ASSERT(oldEntry->Type == LibraryEntryType::File);
            DirectoryEntry* oldDirEntry = static_cast<DirectoryEntry*>(oldEntry);

            DirectoryEntry* newDirEntry = AddDirectoryInternal(newEntryParent, newFullPath).get();
            Stack<std::pair<DirectoryEntry*, DirectoryEntry*>> todos;
            todos.push(std::make_pair(oldDirEntry, newDirEntry));

            while (!todos.empty())
            {
                auto current = todos.top();
                todos.pop();

                DirectoryEntry* sourceDir = current.first;
                DirectoryEntry* dstDir = current.second;

                for (auto& child : sourceDir->Children)
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
        Path fullPath = path;
        if (fullPath.is_absolute())
        {
            if (std::search(path.begin(), path.end(), m_AssetFolder.begin(), m_AssetFolder.end()) == path.end())
                return;
        }
        else
            fullPath = fs::absolute(fullPath);

        if (fs::is_directory(fullPath))
            return;

        fs::create_directory(fullPath);
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

        Path assetPath = path;
        if (path.is_absolute())
        {
            if (std::search(assetPath.begin(), assetPath.end(), m_AssetFolder.begin(), m_AssetFolder.end()) ==
                assetPath.end())
                return;
            assetPath = path.relative_path(); // c++ ppl are stupid
        }

        DeleteEntry(assetPath);
        asset->SetName(path.filename().string());

        // Path absPath = fs::absolute(assetPath);
        Path absPath = assetPath;
        AssetManager::Get().Save(asset, absPath);

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

    Ref<LibraryEntry> ProjectLibrary::FindEntry(const Path& inPath) const
    {
        Path path = inPath.lexically_normal();
        Path relPath;
        const Path* searchPath;
        if (path.is_absolute())
        {
            relPath = fs::relative(path, m_RootEntry->Filepath);
            searchPath = &relPath;
        }
        else
            searchPath = &path;
        Path tmpPath = *searchPath;
        std::vector<Path> paths;
        while (tmpPath != "." && tmpPath.parent_path() != tmpPath)
        {
            paths.push_back(tmpPath.filename());
            tmpPath = tmpPath.parent_path();
        }
        std::reverse(paths.begin(), paths.end());
        uint32_t idx = 0;
        Ref<LibraryEntry> rootLibEntry = m_RootEntry;
        Ref<LibraryEntry>* current = &rootLibEntry;
        while (current != nullptr)
        {
            if (idx == paths.size())
                return *current;

            String cur = (fs::is_regular_file(*searchPath) && idx == (paths.size() - 1))
                           ? searchPath->filename().string()
                           : paths[idx].string();
            if ((*current)->Type == LibraryEntryType::Directory)
            {
                DirectoryEntry* dirEntry = static_cast<DirectoryEntry*>(current->get());
                String copy = cur;
                StringUtils::ToLower(copy);
                size_t curElemHash = Hash(copy);
                current = nullptr;
                for (auto& child : dirEntry->Children)
                {
                    if (curElemHash != child->ElementNameHash)
                        continue;
                    if (cur == child->ElementName)
                    {
                        idx++;
                        current = &child;
                        break;
                    }
                }
            }
            else
            {
                if (idx == (paths.size() - 1))
                    return *current;
                else
                    break;
            }
        }

        return nullptr;
    }

    void ProjectLibrary::CreateInternalParentHierarchy(const Path& path, DirectoryEntry** newHierarchyRoot,
                                                       DirectoryEntry** newHierarchyLeaf)
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
                for (auto& child : dirEntry->Children)
                    makeRelative(child.get(), root);
            }
        };

        makeRelative(m_RootEntry.get(), m_AssetFolder);
    }

    void ProjectLibrary::MakeEntriesAbsolute()
    {
        std::function<void(LibraryEntry*)> makeAbsolute = [&](LibraryEntry* entry) {
            entry->Filepath = (m_AssetFolder / entry->Filepath).lexically_normal();
            if (entry->Type == LibraryEntryType::Directory)
            {
                DirectoryEntry* dirEntry = static_cast<DirectoryEntry*>(entry);
                for (auto& child : dirEntry->Children)
                    makeAbsolute(child.get());
            }
        };

        makeAbsolute(m_RootEntry.get());
    }

    void ProjectLibrary::LoadLibrary()
    {
        UnloadLibrary();

        m_ProjectFolder = Editor::Get().GetProjectPath();
        m_AssetFolder = m_ProjectFolder / ASSET_DIR;
        m_RootEntry = CreateRef<DirectoryEntry>(m_AssetFolder, m_AssetFolder.filename().string(), nullptr);
        m_UuidDirectory = m_ProjectFolder / INTERNAL_ASSET_DIR;

        Path libEntriesPath = m_ProjectFolder / PROJECT_INTERNAL_DIR / LIBRARY_ENTRIES_FILENAME;

        if (fs::exists(libEntriesPath))
        {
            m_RootEntry = DeserializeLibraryEntries(libEntriesPath);
            m_RootEntry->Parent = nullptr;
        }

        String tabs;
        std::function<void(const Ref<LibraryEntry>&)> traverse = [&](const Ref<LibraryEntry>& entry) {
            CW_ENGINE_INFO("{0} Entry: {1}, {2}", tabs, entry->Filepath, entry->ElementName);
            if (entry->Type == LibraryEntryType::Directory)
            {
                tabs += "\t";
                for (auto& child : std::static_pointer_cast<DirectoryEntry>(entry)->Children)
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
        else
            m_AssetManifest = CreateRef<AssetManifest>("ProjectLibrary");

        AssetManager::Get().RegisterAssetManifest(m_AssetManifest);

        Stack<DirectoryEntry*> todos; // Load meta files
        todos.push(m_RootEntry.get());
        Vector<Ref<LibraryEntry>> deletedEntries;
        while (!todos.empty())
        {
            DirectoryEntry* curDir = todos.top();
            todos.pop();

            for (auto& child : curDir->Children)
            {
                if (child->Type == LibraryEntryType::File)
                {
                    Ref<FileEntry> entry = std::static_pointer_cast<FileEntry>(child);
                    if (fs::is_regular_file(entry->Filepath))
                    {
                        if (entry->Metadata == nullptr)
                        {
                            Path metaPath = entry->Filepath;
                            metaPath = metaPath.replace_filename(metaPath.filename().string() + ".meta");
                            if (fs::is_regular_file(metaPath))
                                entry->Metadata = DeserializeMetadata(metaPath);
                        }

                        if (entry->Metadata != nullptr) // if we loaded metadata
                            m_UuidToPath[entry->Metadata->Uuid] = entry->Filepath;
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

        for (auto& deletedEntry : deletedEntries)
        {
            if (deletedEntry->Type == LibraryEntryType::File)
                DeleteAssetInternal(std::static_pointer_cast<FileEntry>(deletedEntry));
            else
                DeleteDirectoryInternal(std::static_pointer_cast<DirectoryEntry>(deletedEntry));
        }

        Path internalAssetFolder = m_AssetFolder / INTERNAL_ASSET_DIR;
        if (fs::exists(internalAssetFolder))
        {
            Vector<Path> toDelete;
            auto processFile = [&](const Path& path) {
                UUID uuid = UUID(path.filename().replace_extension("").string());
                if (m_UuidToPath.find(uuid) != m_UuidToPath.end())
                {
                    m_AssetManifest->UnregisterAsset(uuid);
                    toDelete.push_back(path);
                }
            };

            for (auto fileIterator : fs::recursive_directory_iterator(internalAssetFolder))
            {
                if (fileIterator.is_regular_file())
                    processFile(fileIterator.path());
            }

            for (auto& entry : toDelete)
                fs::remove(entry);
        }

        m_IsLoaded = true;
    }

    void ProjectLibrary::UnloadLibrary()
    {
        if (!m_IsLoaded)
            return;
        m_AssetFolder = Path();
        m_ProjectFolder = Path();
        ClearEntries();
        m_RootEntry = CreateRef<DirectoryEntry>(m_AssetFolder, m_AssetFolder.filename().string(), nullptr);
        AssetManager::Get().UnregisterAssetManifest(m_AssetManifest);
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
                for (auto& child : std::static_pointer_cast<DirectoryEntry>(entry)->Children)
                    traverse(child);
                tabs = tabs.substr(0, tabs.size() - 2);
            }
        };
        // CW_ENGINE_INFO("Original entries");
        // traverse(m_RootEntry);

        MakeEntriesRelative();
        // CW_ENGINE_INFO("Relative entries");
        // traverse(m_RootEntry);
        Path libEntriesPath = m_ProjectFolder / PROJECT_INTERNAL_DIR / LIBRARY_ENTRIES_FILENAME;
        SerializeLibraryEntries(libEntriesPath);
        MakeEntriesAbsolute();
        // CW_ENGINE_INFO("Absolute entries");
        // traverse(m_RootEntry);
        Path assetManifestPath = m_ProjectFolder / PROJECT_INTERNAL_DIR / ASSET_MANIFEST_FILENAME;
        AssetManifest::Serialize(m_AssetManifest, assetManifestPath, m_ProjectFolder);
    }

} // namespace Crowny

#include "cwepch.h"

#include "Editor/AssetLibraryServices.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/Serialization/FileEncoder.h"
#include "Crowny/Serialization/ImportOptionsSerializer.h"

#include <regex>
#include <yaml-cpp/yaml.h>

CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::DirectoryEntry, "DirectoryEntry");
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::FileEntry, "FileEntry");
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::LibraryEntry, Crowny::DirectoryEntry)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::LibraryEntry, Crowny::FileEntry)

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

    void Save(BinaryDataStreamOutputArchive& archive, const FileEntry& entry) { archive(cereal::base_class<LibraryEntry>(&entry), entry.Filesize); }

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
        for (const Ref<LibraryEntry>& child : entry.Children)
            child->Parent = &entry;
    }

    namespace
    {
        constexpr uint32_t PROJECT_METADATA_VERSION = 1;

        Path NormalizePathForComparison(const Path& path)
        {
            Path normalized = fs::absolute(path).lexically_normal();
#ifdef CW_PLATFORM_WIN32
            String value = normalized.generic_string();
            StringUtils::ToLower(value);
            normalized = Path(value);
#endif
            return normalized;
        }

        uint32_t PathDepth(const Path& path) { return (uint32_t)std::distance(path.begin(), path.end()); }

        bool HasMissingParent(const UnorderedSet<Path, HashPath>& missing, const Path& path)
        {
            Path parent = path.parent_path();
            while (parent != parent.parent_path())
            {
                if (missing.find(parent) != missing.end())
                    return true;
                parent = parent.parent_path();
            }
            return false;
        }
    } // namespace

    void AssetIndex::SetRoot(const Ref<DirectoryEntry>& root) { m_Root = root; }

    void AssetIndex::Clear()
    {
        m_Root = nullptr;
        m_UuidToPath.clear();
    }

    Ref<LibraryEntry> AssetIndex::FindEntry(const Path& inputPath) const
    {
        if (m_Root == nullptr)
            return nullptr;

        const Path path = inputPath.lexically_normal();
        Path relativePath = path;
        if (path.is_absolute())
        {
            relativePath = path.lexically_relative(m_Root->Filepath.lexically_normal());
            if (relativePath.empty())
                return nullptr;
        }

        if (relativePath == Path("."))
            return m_Root;
        const auto first = relativePath.begin();
        if (first != relativePath.end() && *first == Path(".."))
            return nullptr;

        Ref<LibraryEntry> current = m_Root;
        for (const Path& segment : relativePath)
        {
            if (segment == Path("."))
                continue;
            if (current == nullptr || current->Type != LibraryEntryType::Directory)
                return nullptr;

            const Ref<DirectoryEntry> directory = StaticRefCast<DirectoryEntry>(current);
            const String name = segment.string();
            String lowerName = name;
            StringUtils::ToLower(lowerName);
            const size_t nameHash = Hash(lowerName);

            current = nullptr;
            for (const Ref<LibraryEntry>& child : directory->Children)
            {
                if (child->ElementNameHash == nameHash && child->ElementName == name)
                {
                    current = child;
                    break;
                }
            }
        }
        return current;
    }

    Ref<FileEntry> AssetIndex::AddFile(DirectoryEntry* parent, const Path& path)
    {
        if (parent == nullptr)
            return nullptr;
        Ref<FileEntry> file = CreateRef<FileEntry>(path, path.filename().string(), parent);
        parent->Children.push_back(file);
        return file;
    }

    Ref<DirectoryEntry> AssetIndex::AddDirectory(DirectoryEntry* parent, const Path& path)
    {
        if (parent == nullptr)
            return nullptr;
        Ref<DirectoryEntry> directory = CreateRef<DirectoryEntry>(path, path.filename().string(), parent);
        parent->Children.push_back(directory);
        return directory;
    }

    void AssetIndex::Remove(const Ref<LibraryEntry>& entry)
    {
        if (entry == nullptr)
            return;
        if (entry == m_Root)
        {
            m_Root = nullptr;
            return;
        }

        DirectoryEntry* parent = entry->Parent;
        if (parent == nullptr)
            return;
        const auto found = std::find(parent->Children.begin(), parent->Children.end(), entry);
        if (found != parent->Children.end())
            parent->Children.erase(found);
        entry->Parent = nullptr;
    }

    Vector<Ref<LibraryEntry>> AssetIndex::Search(const String& pattern, const Vector<AssetType>& assetTypes,
                                                 const Ref<DirectoryEntry>& rootEntry) const
    {
        Vector<Ref<LibraryEntry>> entries;
        if (m_Root == nullptr)
            return entries;

        const std::regex escape("[.^$|()\\[\\]{}*+?\\\\]");
        const String escapedPattern =
          std::regex_replace(pattern, escape, "\\\\&", std::regex_constants::match_default | std::regex_constants::format_sed);
#ifdef CW_PLATFORM_WIN32
        const std::regex wildcard("\\\\\\*");
#else
        const std::regex wildcard("\\\\\\\\\\*");
#endif
        const String searchPattern = std::regex_replace(escapedPattern, wildcard, ".*");
        const std::regex searchRegex(searchPattern, std::regex_constants::ECMAScript | std::regex_constants::icase);

        Stack<DirectoryEntry*> pending;
        pending.push(rootEntry != nullptr ? rootEntry.get() : m_Root.get());
        while (!pending.empty())
        {
            DirectoryEntry* directory = pending.top();
            pending.pop();
            for (const Ref<LibraryEntry>& child : directory->Children)
            {
                if (std::regex_match(child->ElementName, searchRegex))
                {
                    if (assetTypes.empty())
                        entries.push_back(child);
                    else if (child->Type == LibraryEntryType::File)
                    {
                        const Ref<FileEntry> file = StaticRefCast<FileEntry>(child);
                        if (file->Metadata != nullptr && std::find(assetTypes.begin(), assetTypes.end(), file->Metadata->Type) != assetTypes.end())
                            entries.push_back(child);
                    }
                }
                if (child->Type == LibraryEntryType::Directory)
                    pending.push(static_cast<DirectoryEntry*>(child.get()));
            }
        }
        return entries;
    }

    void AssetIndex::Register(const UUID& uuid, const Path& sourcePath)
    {
        if (!uuid.Empty())
            m_UuidToPath[uuid] = sourcePath;
    }

    void AssetIndex::Unregister(const UUID& uuid) { m_UuidToPath.erase(uuid); }

    Path AssetIndex::UuidToPath(const UUID& uuid) const
    {
        const auto found = m_UuidToPath.find(uuid);
        return found != m_UuidToPath.end() ? found->second : Path();
    }

    AssetType AssetIndex::GetAssetType(const Path& path) const
    {
        const Ref<LibraryEntry> entry = FindEntry(path);
        if (entry == nullptr || entry->Type != LibraryEntryType::File)
            return AssetType::None;
        const Ref<FileEntry> file = StaticRefCast<FileEntry>(entry);
        return file->Metadata != nullptr ? file->Metadata->Type : AssetType::None;
    }

    AssetType AssetIndex::GetAssetType(const UUID& uuid) const
    {
        const Path path = UuidToPath(uuid);
        return path.empty() ? AssetType::None : GetAssetType(path);
    }

    String AssetIndex::GetAssetName(const UUID& uuid) const
    {
        const Path path = UuidToPath(uuid);
        if (path.empty())
            return {};
        const Ref<LibraryEntry> entry = FindEntry(path);
        return entry != nullptr ? entry->ElementName : path.stem().string();
    }

    bool AssetFileSystemDiff::Empty() const
    {
        return AddedDirectories.empty() && AddedFiles.empty() && RemovedEntries.empty() && FilesToImport.empty() && DanglingMetadata.empty();
    }

    Path AssetFileSystemScanner::ResolvePath(const Path& assetRoot, const Path& path)
    {
        return (path.is_absolute() ? path : assetRoot / path).lexically_normal();
    }

    bool AssetFileSystemScanner::IsPathWithin(const Path& root, const Path& candidate)
    {
        if (root.empty() || candidate.empty())
            return false;
        const Path normalizedRoot = NormalizePathForComparison(root);
        const Path normalizedCandidate = NormalizePathForComparison(candidate);
        const auto mismatch = std::mismatch(normalizedRoot.begin(), normalizedRoot.end(), normalizedCandidate.begin(), normalizedCandidate.end());
        return mismatch.first == normalizedRoot.end();
    }

    bool AssetFileSystemScanner::IsMetadata(const Path& path) { return path.extension() == ".meta"; }

    Path AssetFileSystemScanner::GetMetadataPath(const Path& path)
    {
        Path metadataPath = path;
        return metadataPath.replace_extension(metadataPath.extension().string() + ".meta");
    }

    AssetFileSystemDiff AssetFileSystemScanner::Scan(const Path& assetRoot, const Path& requestedPath, const AssetIndex& index) const
    {
        AssetFileSystemDiff diff;
        diff.ScanRoot = ResolvePath(assetRoot, requestedPath);
        if (!IsPathWithin(assetRoot, diff.ScanRoot))
            return diff;

        UnorderedMap<Path, LibraryEntryType, HashPath> diskEntries;
        auto inspectFile = [&](const Path& path) {
            if (!IsMetadata(path))
            {
                diskEntries[path.lexically_normal()] = LibraryEntryType::File;
                return;
            }
            Path sourcePath = path;
            sourcePath.replace_extension("");
            if (!fs::is_regular_file(sourcePath))
                diff.DanglingMetadata.push_back(path);
        };

        if (fs::is_regular_file(diff.ScanRoot))
            inspectFile(diff.ScanRoot);
        else if (fs::is_directory(diff.ScanRoot))
        {
            diskEntries[diff.ScanRoot] = LibraryEntryType::Directory;
            std::error_code error;
            fs::recursive_directory_iterator iterator(diff.ScanRoot, fs::directory_options::skip_permission_denied, error);
            const fs::recursive_directory_iterator end;
            while (iterator != end)
            {
                if (error)
                {
                    CW_ENGINE_WARN("Failed to scan '{}': {}", diff.ScanRoot, error.message());
                    error.clear();
                    iterator.increment(error);
                    continue;
                }
                const fs::directory_entry& entry = *iterator;
                if (entry.is_directory(error))
                    diskEntries[entry.path().lexically_normal()] = LibraryEntryType::Directory;
                else if (entry.is_regular_file(error))
                    inspectFile(entry.path().lexically_normal());
                iterator.increment(error);
            }
        }

        UnorderedMap<Path, Ref<LibraryEntry>, HashPath> indexedEntries;
        const Ref<LibraryEntry> indexedRoot = index.FindEntry(diff.ScanRoot);
        if (indexedRoot != nullptr)
        {
            Stack<Ref<LibraryEntry>> pending;
            pending.push(indexedRoot);
            while (!pending.empty())
            {
                Ref<LibraryEntry> entry = pending.top();
                pending.pop();
                indexedEntries[entry->Filepath.lexically_normal()] = entry;
                if (entry->Type == LibraryEntryType::Directory)
                {
                    for (const Ref<LibraryEntry>& child : StaticRefCast<DirectoryEntry>(entry)->Children)
                        pending.push(child);
                }
            }
        }

        UnorderedSet<Path, HashPath> missingIndexedPaths;
        for (const auto& [path, entry] : indexedEntries)
        {
            const auto disk = diskEntries.find(path);
            if (disk == diskEntries.end() || disk->second != entry->Type)
                missingIndexedPaths.insert(path);
        }
        for (const auto& [path, entry] : indexedEntries)
        {
            if (missingIndexedPaths.find(path) != missingIndexedPaths.end() && !HasMissingParent(missingIndexedPaths, path))
                diff.RemovedEntries.push_back(entry);
        }

        for (const auto& [path, type] : diskEntries)
        {
            const auto indexed = indexedEntries.find(path);
            if (indexed == indexedEntries.end() || indexed->second->Type != type)
            {
                if (type == LibraryEntryType::Directory && path != assetRoot.lexically_normal())
                    diff.AddedDirectories.push_back(path);
                else if (type == LibraryEntryType::File)
                    diff.AddedFiles.push_back(path);
            }
            else if (type == LibraryEntryType::File)
                diff.FilesToImport.push_back(StaticRefCast<FileEntry>(indexed->second));
        }

        std::sort(diff.AddedDirectories.begin(), diff.AddedDirectories.end(),
                  [](const Path& left, const Path& right) { return PathDepth(left) < PathDepth(right); });
        std::sort(diff.AddedFiles.begin(), diff.AddedFiles.end());
        return diff;
    }

    void AssetMetadataStore::Save(const Path& path, const Ref<AssetMetadata>& metadata, const Vector<Ref<AssetMetadata>>& dependents) const
    {
        if (metadata == nullptr)
            return;
        YAML::Emitter output;
        output << YAML::BeginMap;
        output << YAML::Key << "Version" << YAML::Value << PROJECT_METADATA_VERSION;
        output << YAML::Key << "Uuid" << YAML::Value << metadata->Uuid;
        output << YAML::Key << "IncludeInBuild" << YAML::Value << metadata->IncludeInBuild;
        output << YAML::Key << "TypeId" << YAML::Value << (uint32_t)metadata->Type;
        ImportOptionsSerializer::Serialize(output, metadata->ImportOptions);
        if (!dependents.empty())
        {
            output << YAML::Key << "Dependents" << YAML::Value << YAML::BeginSeq;
            for (const Ref<AssetMetadata>& dependent : dependents)
            {
                output << YAML::BeginMap;
                output << YAML::Key << "Uuid" << YAML::Value << dependent->Uuid;
                output << YAML::Key << "TypeId" << YAML::Value << (uint32_t)dependent->Type;
                output << YAML::EndMap;
            }
            output << YAML::EndSeq;
        }
        output << YAML::EndMap;
        if (!fs::is_directory(path.parent_path()))
            fs::create_directories(path.parent_path());
        FileSystem::WriteTextFile(path, output.c_str());
    }

    Ref<AssetMetadata> AssetMetadataStore::Load(const Path& path, Vector<Ref<AssetMetadata>>* outDependents) const
    {
        if (outDependents != nullptr)
            outDependents->clear();
        if (!fs::is_regular_file(path))
            return nullptr;
        try
        {
            const Ref<DataStream> stream = FileSystem::OpenFile(path);
            const String text = stream->GetAsString();
            stream->Close();
            const YAML::Node data = YAML::Load(text);
            if (!data || !data.IsMap())
                return nullptr;

            const uint32_t version = data["Version"].as<uint32_t>(0);
            if (version > PROJECT_METADATA_VERSION)
            {
                CW_ENGINE_ERROR("Metadata '{}' uses unsupported version {}.", path, version);
                return nullptr;
            }

            Ref<AssetMetadata> metadata = CreateRef<AssetMetadata>();
            if (const auto& uuid = data["Uuid"])
                metadata->Uuid = uuid.as<UUID>();
            if (metadata->Uuid.Empty())
            {
                CW_ENGINE_WARN("Metadata '{}' has no valid UUID. Generating a replacement.", path);
                metadata->Uuid = UuidGenerator::Generate();
            }
            metadata->IncludeInBuild = data["IncludeInBuild"].as<bool>(false);
            const uint32_t type = data["TypeId"].as<uint32_t>((uint32_t)AssetType::None);
            if (type <= (uint32_t)AssetType::AnimationClip)
                metadata->Type = (AssetType)type;
            else
                CW_ENGINE_WARN("Metadata '{}' has invalid asset type {}.", path, type);
            metadata->ImportOptions = ImportOptionsSerializer::Deserialize(data);

            if (outDependents != nullptr)
            {
                const YAML::Node dependents = data["Dependents"];
                if (dependents && dependents.IsSequence())
                {
                    for (const YAML::Node& node : dependents)
                    {
                        if (!node.IsMap())
                            continue;
                        Ref<AssetMetadata> dependent = CreateRef<AssetMetadata>();
                        dependent->Uuid = node["Uuid"].as<UUID>(UUID::EMPTY);
                        const uint32_t dependentType = node["TypeId"].as<uint32_t>((uint32_t)AssetType::None);
                        if (dependent->Uuid.Empty() || dependentType > (uint32_t)AssetType::AnimationClip)
                            continue;
                        dependent->Type = (AssetType)dependentType;
                        dependent->IncludeInBuild = true;
                        outDependents->push_back(dependent);
                    }
                }
            }
            return metadata;
        }
        catch (const std::exception& error)
        {
            CW_ENGINE_ERROR("Failed to read metadata '{}': {}", path, error.what());
            return nullptr;
        }
    }

    void AssetMetadataStore::SaveIndex(const Path& path, const Ref<DirectoryEntry>& root) const
    {
        if (!fs::is_directory(path.parent_path()))
            fs::create_directories(path.parent_path());
        FileEncoder<DirectoryEntry, SerializerType::Binary> encoder(path);
        encoder.Encode(root);
    }

    Ref<DirectoryEntry> AssetMetadataStore::LoadIndex(const Path& path) const
    {
        try
        {
            FileDecoder<DirectoryEntry, SerializerType::Binary> decoder(path);
            return decoder.Decode();
        }
        catch (const std::exception& error)
        {
            CW_ENGINE_ERROR("Failed to load asset index '{}': {}", path, error.what());
            return nullptr;
        }
    }

    bool AssetFilesystemOperations::Move(const Path& source, const Path& destination, bool overwrite) const
    {
        if (!fs::exists(source) || (!overwrite && fs::exists(destination)))
            return false;
        std::error_code error;
        if (overwrite && fs::exists(destination))
            fs::remove_all(destination, error);
        if (!error)
            fs::rename(source, destination, error);
        if (error)
            CW_ENGINE_ERROR("Failed to move '{}' to '{}': {}", source, destination, error.message());
        return !error;
    }

    bool AssetFilesystemOperations::CopyFile(const Path& source, const Path& destination, bool overwrite) const
    {
        std::error_code error;
        const fs::copy_options options = overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::none;
        const bool copied = fs::copy_file(source, destination, options, error);
        if (error)
            CW_ENGINE_ERROR("Failed to copy '{}' to '{}': {}", source, destination, error.message());
        return copied && !error;
    }

    bool AssetFilesystemOperations::CopyDirectory(const Path& source, const Path& destination, bool overwrite) const
    {
        std::error_code error;
        fs::copy_options options = fs::copy_options::recursive;
        options |= overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::skip_existing;
        fs::copy(source, destination, options, error);
        if (error)
            CW_ENGINE_ERROR("Failed to copy '{}' to '{}': {}", source, destination, error.message());
        return !error;
    }

    bool AssetFilesystemOperations::CreateDirectory(const Path& path) const
    {
        std::error_code error;
        const bool created = fs::create_directories(path, error);
        return !error && (created || fs::is_directory(path));
    }

    bool AssetFilesystemOperations::Remove(const Path& path) const
    {
        std::error_code error;
        const uintmax_t removed = fs::remove_all(path, error);
        if (error)
            CW_ENGINE_ERROR("Failed to remove '{}': {}", path, error.message());
        return !error && removed > 0;
    }

    bool BuildManifestSelection::SetIncluded(const Ref<FileEntry>& entry, bool include, const AssetMetadataStore& metadataStore) const
    {
        if (entry == nullptr || entry->Metadata == nullptr || entry->Metadata->IncludeInBuild == include)
            return false;
        entry->Metadata->IncludeInBuild = include;
        metadataStore.Save(AssetFileSystemScanner::GetMetadataPath(entry->Filepath), entry->Metadata, entry->DependentMetadata);
        return true;
    }

    Vector<Ref<FileEntry>> BuildManifestSelection::Collect(const Ref<DirectoryEntry>& root) const
    {
        Vector<Ref<FileEntry>> output;
        if (root == nullptr)
            return output;
        Stack<DirectoryEntry*> pending;
        pending.push(root.get());
        while (!pending.empty())
        {
            DirectoryEntry* directory = pending.top();
            pending.pop();
            for (const Ref<LibraryEntry>& child : directory->Children)
            {
                if (child->Type == LibraryEntryType::File)
                {
                    const Ref<FileEntry> file = StaticRefCast<FileEntry>(child);
                    if (file->Metadata != nullptr && file->Metadata->IncludeInBuild)
                        output.push_back(file);
                }
                else
                    pending.push(static_cast<DirectoryEntry*>(child.get()));
            }
        }
        return output;
    }

} // namespace Crowny

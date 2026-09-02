#include "cwepch.h"

#include "Editor/AssetLibraryServices.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/Serialization/FileEncoder.h"
#include "Crowny/Serialization/ImportOptionsSerializer.h"

#include "Panels/ViewportHudText.h"

#include <regex>
#include <yaml-cpp/yaml.h>

CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::DirectoryEntry, "DirectoryEntry");
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::FileEntry, "FileEntry");
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::LibraryEntry, Crowny::DirectoryEntry)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::LibraryEntry, Crowny::FileEntry)

namespace Crowny
{
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
        constexpr uint32_t LEGACY_PROJECT_METADATA_VERSION = 1;
        constexpr uint32_t PROJECT_METADATA_VERSION = 2;

        struct ParsedAssetMetadata
        {
            Ref<AssetMetadata> Metadata;
            Vector<Ref<AssetMetadata>> Dependents;
            uint32_t Version = 0;
            String Error;

            explicit operator bool() const { return Metadata != nullptr; }
        };

        Path GetMetadataBackupPath(const Path& path) { return Path(path.string() + ".bak"); }

        bool IsMetadataBackup(const Path& path) { return path.extension() == ".bak" && path.stem().extension() == ".meta"; }

        bool IsPersistableAssetType(uint32_t type)
        {
            return type > static_cast<uint32_t>(AssetType::None) && type <= static_cast<uint32_t>(AssetType::AnimationClip);
        }

        bool ReadText(const Path& path, String& output, String& error)
        {
            if (!fs::is_regular_file(path))
            {
                error = "Cannot open metadata file.";
                return false;
            }
            output = FileSystem::ReadTextFile(path);
            if (output.empty())
            {
                error = "Metadata file is empty or unreadable.";
                return false;
            }
            return true;
        }

        ParsedAssetMetadata ParseMetadata(StringView text)
        {
            ParsedAssetMetadata result;
            try
            {
                const YAML::Node data = YAML::Load(String(text));
                if (!data || !data.IsMap())
                {
                    result.Error = "Metadata root must be a map.";
                    return result;
                }

                const YAML::Node versionNode = data["Version"];
                if (!versionNode || !versionNode.IsScalar())
                {
                    result.Error = "Metadata is missing its format version.";
                    return result;
                }
                result.Version = versionNode.as<uint32_t>();
                if (result.Version != LEGACY_PROJECT_METADATA_VERSION && result.Version != PROJECT_METADATA_VERSION)
                {
                    result.Error = "Metadata version " + std::to_string(result.Version) + " is not supported.";
                    return result;
                }

                const YAML::Node uuidNode = data["Uuid"];
                const YAML::Node includeNode = data["IncludeInBuild"];
                const YAML::Node typeNode = data["TypeId"];
                if (!uuidNode || !uuidNode.IsScalar() || !includeNode || !includeNode.IsScalar() || !typeNode || !typeNode.IsScalar())
                {
                    result.Error = "Metadata is missing a required identity field.";
                    return result;
                }

                Ref<AssetMetadata> metadata = CreateRef<AssetMetadata>();
                metadata->Uuid = uuidNode.as<UUID>(UUID::EMPTY);
                metadata->IncludeInBuild = includeNode.as<bool>();
                const uint32_t type = typeNode.as<uint32_t>();
                if (metadata->Uuid.Empty() || !IsPersistableAssetType(type))
                {
                    result.Error = "Metadata has an invalid UUID or asset type.";
                    return result;
                }
                metadata->Type = static_cast<AssetType>(type);
                metadata->ImportOptions = ImportOptionsSerializer::Deserialize(data);
                if (metadata->ImportOptions == nullptr)
                {
                    result.Error = "Metadata import options could not be decoded.";
                    return result;
                }

                UnorderedSet<UUID> uuids{ metadata->Uuid };
                UnorderedSet<String> keys;
                const YAML::Node dependents = data["Dependents"];
                if (dependents && !dependents.IsSequence())
                {
                    result.Error = "Metadata dependents must be a sequence.";
                    return result;
                }
                if (dependents)
                {
                    for (const YAML::Node& node : dependents)
                    {
                        if (!node.IsMap())
                        {
                            result.Error = "A dependent metadata entry is not a map.";
                            return result;
                        }

                        const YAML::Node dependentUuidNode = node["Uuid"];
                        const YAML::Node dependentTypeNode = node["TypeId"];
                        if (!dependentUuidNode || !dependentUuidNode.IsScalar() || !dependentTypeNode || !dependentTypeNode.IsScalar())
                        {
                            result.Error = "A dependent metadata entry is missing its UUID or asset type.";
                            return result;
                        }

                        Ref<AssetMetadata> dependent = CreateRef<AssetMetadata>();
                        dependent->Uuid = dependentUuidNode.as<UUID>(UUID::EMPTY);
                        const uint32_t dependentType = dependentTypeNode.as<uint32_t>();
                        if (dependent->Uuid.Empty() || !IsPersistableAssetType(dependentType) || !uuids.insert(dependent->Uuid).second)
                        {
                            result.Error = "A dependent metadata entry has an invalid or duplicate identity.";
                            return result;
                        }
                        dependent->Type = static_cast<AssetType>(dependentType);
                        dependent->IncludeInBuild = true;

                        if (result.Version >= PROJECT_METADATA_VERSION)
                        {
                            const YAML::Node keyNode = node["Key"];
                            if (!keyNode || !keyNode.IsScalar())
                            {
                                result.Error = "A version-two dependent metadata entry is missing its stable key.";
                                return result;
                            }
                            dependent->SubassetKey = keyNode.as<String>();
                            if (dependent->SubassetKey.empty() || !keys.insert(dependent->SubassetKey).second)
                            {
                                result.Error = "A dependent metadata entry has an empty or duplicate stable key.";
                                return result;
                            }
                        }
                        result.Dependents.push_back(std::move(dependent));
                    }
                }

                result.Metadata = std::move(metadata);
                return result;
            }
            catch (const std::exception& error)
            {
                result.Metadata = nullptr;
                result.Dependents.clear();
                result.Error = error.what();
                return result;
            }
        }

        ParsedAssetMetadata ParseMetadataFile(const Path& path)
        {
            String text;
            String error;
            if (!ReadText(path, text, error))
            {
                ParsedAssetMetadata result;
                result.Error = std::move(error);
                return result;
            }
            return ParseMetadata(text);
        }

        String BuildSubassetBaseKey(const Ref<Asset>& asset)
        {
            String name = asset != nullptr ? asset->GetName() : String();
            std::replace(name.begin(), name.end(), '\\', '/');
            if (name.empty())
                name = "<unnamed>";
            return std::to_string(static_cast<uint32_t>(asset->GetAssetType())) + ":" + name;
        }

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

    bool AssetFileSystemScanner::IsMetadata(const Path& path) { return path.extension() == ".meta" || IsMetadataBackup(path); }

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
            if (IsMetadataBackup(path))
            {
                Path primaryPath = path;
                primaryPath.replace_extension("");
                Path sourcePath = primaryPath;
                sourcePath.replace_extension("");
                if (!fs::is_regular_file(primaryPath) && !fs::is_regular_file(sourcePath))
                    diff.DanglingMetadata.push_back(primaryPath);
                return;
            }
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

    bool AssetMetadataStore::Save(const Path& path, const Ref<AssetMetadata>& metadata, const Vector<Ref<AssetMetadata>>& dependents,
                                  String* outError) const
    {
        if (outError != nullptr)
            outError->clear();
        if (metadata == nullptr || metadata->Uuid.Empty() || !IsPersistableAssetType(static_cast<uint32_t>(metadata->Type)) ||
            metadata->ImportOptions == nullptr)
        {
            if (outError != nullptr)
                *outError = "Cannot save metadata without a valid primary UUID, asset type, and import options.";
            return false;
        }

        UnorderedSet<UUID> uuids{ metadata->Uuid };
        UnorderedSet<String> keys;
        for (const Ref<AssetMetadata>& dependent : dependents)
        {
            if (dependent == nullptr || dependent->Uuid.Empty() || dependent->SubassetKey.empty() ||
                !IsPersistableAssetType(static_cast<uint32_t>(dependent->Type)) || !uuids.insert(dependent->Uuid).second ||
                !keys.insert(dependent->SubassetKey).second)
            {
                if (outError != nullptr)
                    *outError = "Cannot save dependent metadata with an invalid or duplicate UUID, type, or stable key.";
                return false;
            }
        }

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
                output << YAML::Key << "Key" << YAML::Value << dependent->SubassetKey;
                output << YAML::Key << "Uuid" << YAML::Value << dependent->Uuid;
                output << YAML::Key << "TypeId" << YAML::Value << (uint32_t)dependent->Type;
                output << YAML::EndMap;
            }
            output << YAML::EndSeq;
        }
        output << YAML::EndMap;
        if (!output.good())
        {
            if (outError != nullptr)
                *outError = output.GetLastError();
            return false;
        }

        const String serialized = output.c_str();
        const ParsedAssetMetadata validation = ParseMetadata(serialized);
        if (!validation)
        {
            if (outError != nullptr)
                *outError = "Generated metadata failed validation: " + validation.Error;
            return false;
        }

        if (fs::is_regular_file(path))
        {
            String previousText;
            String readError;
            if (ReadText(path, previousText, readError) && ParseMetadata(previousText))
            {
                String backupError;
                if (!FileSystem::WriteTextFileAtomic(GetMetadataBackupPath(path), previousText, &backupError))
                {
                    if (outError != nullptr)
                        *outError = "Failed to preserve the last-good metadata: " + backupError;
                    return false;
                }
            }
        }

        return FileSystem::WriteTextFileAtomic(path, serialized, outError);
    }

    AssetMetadataStore::LoadResult AssetMetadataStore::Load(const Path& path) const
    {
        LoadResult result;
        const Path backupPath = GetMetadataBackupPath(path);
        const bool primaryExists = fs::is_regular_file(path);
        const bool backupExists = fs::is_regular_file(backupPath);
        if (!primaryExists && !backupExists)
            return result;

        ParsedAssetMetadata primary;
        if (primaryExists)
            primary = ParseMetadataFile(path);
        if (primary)
        {
            result.Status = AssetMetadataLoadStatus::Loaded;
            result.Metadata = std::move(primary.Metadata);
            result.Dependents = std::move(primary.Dependents);
            result.Version = primary.Version;
            return result;
        }

        ParsedAssetMetadata backup;
        if (backupExists)
            backup = ParseMetadataFile(backupPath);
        if (backup)
        {
            result.Status = AssetMetadataLoadStatus::Recovered;
            result.Metadata = std::move(backup.Metadata);
            result.Dependents = std::move(backup.Dependents);
            result.Version = backup.Version;
            result.Error = primaryExists ? primary.Error : "The primary metadata file is missing.";
            return result;
        }

        result.Status = AssetMetadataLoadStatus::Corrupt;
        result.Error = primaryExists ? primary.Error : "The primary metadata file is missing.";
        if (backupExists)
            result.Error += " Backup recovery failed: " + backup.Error;
        return result;
    }

    AssetMetadataStore::DependentReconciliation AssetMetadataStore::ReconcileDependents(const Vector<Ref<AssetMetadata>>& existing,
                                                                                        const Vector<Ref<Asset>>& importedAssets) const
    {
        DependentReconciliation result;
        UnorderedMap<String, Ref<AssetMetadata>> keyed;
        Map<AssetType, Vector<Ref<AssetMetadata>>> legacyByType;
        for (const Ref<AssetMetadata>& metadata : existing)
        {
            if (metadata == nullptr || metadata->Uuid.Empty())
                continue;
            if (metadata->SubassetKey.empty())
                legacyByType[metadata->Type].push_back(metadata);
            else if (keyed.find(metadata->SubassetKey) == keyed.end())
                keyed.emplace(metadata->SubassetKey, metadata);
        }

        Map<AssetType, size_t> legacyCursor;
        UnorderedMap<String, uint32_t> keyOccurrences;
        UnorderedSet<UUID> reused;
        result.Assignments.reserve(importedAssets.size());
        for (const Ref<Asset>& asset : importedAssets)
        {
            if (asset == nullptr || !IsPersistableAssetType(static_cast<uint32_t>(asset->GetAssetType())))
                continue;

            const String baseKey = BuildSubassetBaseKey(asset);
            const String key = baseKey + "#" + std::to_string(keyOccurrences[baseKey]++);
            Ref<AssetMetadata> previous;
            const auto keyedIter = keyed.find(key);
            if (keyedIter != keyed.end() && keyedIter->second->Type == asset->GetAssetType() && reused.find(keyedIter->second->Uuid) == reused.end())
                previous = keyedIter->second;
            else
            {
                Vector<Ref<AssetMetadata>>& candidates = legacyByType[asset->GetAssetType()];
                size_t& cursor = legacyCursor[asset->GetAssetType()];
                while (cursor < candidates.size() && reused.find(candidates[cursor]->Uuid) != reused.end())
                    cursor++;
                if (cursor < candidates.size())
                    previous = candidates[cursor++];
            }

            Ref<AssetMetadata> metadata = CreateRef<AssetMetadata>();
            metadata->Uuid = previous != nullptr ? previous->Uuid : UuidGenerator::Generate();
            metadata->Type = asset->GetAssetType();
            metadata->IncludeInBuild = true;
            metadata->SubassetKey = key;
            reused.insert(metadata->Uuid);
            result.Assignments.push_back({ asset, std::move(metadata) });
        }

        for (const Ref<AssetMetadata>& metadata : existing)
        {
            if (metadata != nullptr && !metadata->Uuid.Empty() && reused.find(metadata->Uuid) == reused.end())
                result.Orphans.push_back(metadata);
        }
        return result;
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

    bool AssetFilesystemOperations::CopyFileAtomic(const Path& source, const Path& destination, bool overwrite, String* outError) const
    {
        if (outError != nullptr)
            outError->clear();
        if (!fs::is_regular_file(source) || (!overwrite && fs::exists(destination)))
        {
            if (outError != nullptr)
                *outError = "The source is not a regular file or the destination already exists.";
            return false;
        }

        const Ref<DataStream> stream = FileSystem::OpenFile(source);
        if (stream == nullptr || !stream->IsReadable())
        {
            if (outError != nullptr)
                *outError = "The source could not be opened for reading.";
            return false;
        }

        Vector<byte> contents(stream->Size());
        const size_t bytesRead = stream->Read(contents.data(), contents.size());
        stream->Close();
        if (bytesRead != contents.size())
        {
            if (outError != nullptr)
                *outError = "The source could not be read completely.";
            return false;
        }
        return FileSystem::WriteFileAtomic(destination, contents.data(), contents.size(), outError);
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
        const bool previous = entry->Metadata->IncludeInBuild;
        entry->Metadata->IncludeInBuild = include;
        String error;
        if (metadataStore.Save(AssetFileSystemScanner::GetMetadataPath(entry->Filepath), entry->Metadata, entry->DependentMetadata, &error))
            return true;
        entry->Metadata->IncludeInBuild = previous;
        CW_ENGINE_ERROR("Failed to update build inclusion metadata '{}': {}", entry->Filepath, error);
        return false;
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

    namespace
    {
        bool SameFileContents(const Path& left, const Path& right)
        {
            if (!FileSystem::FileExists(left) || !FileSystem::FileExists(right))
                return false;
            return FileSystem::GetFileSize(left) == FileSystem::GetFileSize(right);
        }

        // Picks a destination inside the asset folder for an external file. Re-uses an existing copy with the same
        // size (dropping the same model twice just adds another instance) and otherwise appends " (n)".
        Path ResolveDropDestination(const Path& source, const Path& assetFolder)
        {
            Path destination = assetFolder / source.filename();
            if (!fs::exists(destination) || SameFileContents(source, destination))
                return destination;

            const String stem = source.stem().string();
            const String extension = source.extension().string();
            for (uint32_t index = 1; index < 1000; index++)
            {
                destination = assetFolder / (stem + " (" + std::to_string(index) + ")" + extension);
                if (!fs::exists(destination) || SameFileContents(source, destination))
                    return destination;
            }
            return {};
        }

        bool CopyDropFile(const Path& source, const Path& destination)
        {
            if (fs::exists(destination) && SameFileContents(source, destination))
                return true;
            std::error_code error;
            fs::create_directories(destination.parent_path(), error);
            error.clear();
            fs::copy_file(source, destination, fs::copy_options::overwrite_existing, error);
            if (error)
            {
                CW_ENGINE_ERROR("Failed to import dropped file '{}' into '{}': {}", source, destination, error.message());
                return false;
            }
            return true;
        }
    } // namespace

    // Copies the external file (plus glTF buffers/images and OBJ material libraries it references) into the
    // project's asset folder. Files already inside the asset folder are used in place. Returns the in-project path.
    Path ImportExternalDropFile(const Path& source, const Path& assetFolder)
    {
        std::error_code error;
        if (!fs::is_regular_file(source, error) || error)
            return {};

        const Path normalizedSource = source.lexically_normal();
        if (AssetFileSystemScanner::IsPathWithin(assetFolder, normalizedSource))
            return normalizedSource;

        const Path destination = ResolveDropDestination(normalizedSource, assetFolder);
        if (destination.empty() || !CopyDropFile(normalizedSource, destination))
            return {};

        if (ClassifyViewportDropFile(normalizedSource) == ViewportDropFileKind::Mesh)
        {
            const String contents = FileSystem::ReadTextFile(normalizedSource);
            for (const Path& reference : CollectMeshSidecarReferences(normalizedSource, contents))
            {
                const Path sidecarSource = (normalizedSource.parent_path() / reference).lexically_normal();
                const Path sidecarDestination = (destination.parent_path() / reference).lexically_normal();
                if (!AssetFileSystemScanner::IsPathWithin(assetFolder, sidecarDestination) || !fs::is_regular_file(sidecarSource, error) || error)
                {
                    error.clear();
                    CW_ENGINE_WARN("Dropped mesh '{}' references '{}', which could not be copied alongside it.", normalizedSource, reference);
                    continue;
                }
                CopyDropFile(sidecarSource, sidecarDestination);
            }
        }
        return destination;
    }

} // namespace Crowny

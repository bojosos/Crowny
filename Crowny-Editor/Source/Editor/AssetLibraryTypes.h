#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Import/ImportOptions.h"

#include <atomic>
#include <ctime>

namespace Crowny
{
    struct DirectoryEntry;

    enum class LibraryEntryType
    {
        File,
        Directory
    };

    struct LibraryEntry : public RefCounted
    {
        LibraryEntry() = default;
        LibraryEntry(const Path& path, const String& name, DirectoryEntry* parent, LibraryEntryType type);
        virtual ~LibraryEntry() = default;

        LibraryEntryType Type = LibraryEntryType::File;
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
        ~FileEntry() = default;

        Ref<AssetMetadata> Metadata;
        Vector<Ref<AssetMetadata>> DependentMetadata;
        uint32_t Filesize = 0;
        uint64_t Revision = 0;
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
        Ref<FileEntry> Entry;
        Ref<ImportOptions> Options;
        bool ForceReimport = false;
        bool RunOnMainThread = false;
    };

    struct ImportResult
    {
        ImportTask Task;
        Ref<Asset> Asset;
    };

    struct ImportProgress
    {
        std::atomic<bool> Active{ false };
        std::atomic<uint32_t> TotalFiles{ 0 };
        std::atomic<uint32_t> CompletedFiles{ 0 };
        String CurrentAssetName;

        float GetFraction() const
        {
            const uint32_t total = TotalFiles.load();
            return total > 0 ? (float)CompletedFiles.load() / (float)total : 0.0f;
        }
    };
} // namespace Crowny

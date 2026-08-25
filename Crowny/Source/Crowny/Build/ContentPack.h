#pragma once

#include "Crowny/Build/BuildTypes.h"

#include <optional>
#include <shared_mutex>

namespace Crowny
{
    constexpr uint32_t CONTENT_PACK_FORMAT = 1;

    struct ContentPackDescriptor
    {
        String PackId = "main";
        String EngineVersion;
        uint32_t PlayerAbi = 1;
        uint32_t ContentSchema = 1;
        int32_t MountPriority = 0;
    };

    struct ContentPackInput
    {
        UUID Id;
        Path LogicalPath;
        Path SourcePath;
    };

    struct ContentPackEntry
    {
        UUID Id;
        Path LogicalPath;
        uint64_t Offset = 0;
        uint64_t StoredSize = 0;
        uint64_t UncompressedSize = 0;
        String Sha256;
    };

    String ComputeFileSha256(const Path& path, BuildCancellationCheck cancellation = {});
    String ComputeBytesSha256(const uint8_t* data, size_t size);

    class ContentPackWriter
    {
    public:
        static String Write(const Path& path, const ContentPackDescriptor& descriptor, const Vector<ContentPackInput>& inputs,
                            BuildCancellationCheck cancellation = {});
    };

    class ContentPackReader
    {
    public:
        String Open(const Path& path);
        void Close();

        bool IsOpen() const;
        ContentPackDescriptor GetDescriptor() const;
        Vector<ContentPackEntry> GetEntries() const;
        std::optional<ContentPackEntry> Find(const Path& logicalPath) const;
        std::optional<ContentPackEntry> Find(const UUID& id) const;
        String Read(const Path& logicalPath, Vector<uint8_t>& output) const;
        String Read(const UUID& id, Vector<uint8_t>& output) const;

    private:
        void ResetLocked();
        String ReadLocked(const ContentPackEntry& entry, Vector<uint8_t>& output) const;

        Path m_Path;
        ContentPackDescriptor m_Descriptor;
        Vector<ContentPackEntry> m_Entries;
        UnorderedMap<String, size_t> m_PathIndex;
        UnorderedMap<UUID, size_t> m_UuidIndex;
        uint64_t m_FileSize = 0;
        mutable std::shared_mutex m_Mutex;
        bool m_Open = false;
    };
} // namespace Crowny

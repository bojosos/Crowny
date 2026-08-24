#include "cwpch.h"

#include "Crowny/Common/BuiltInResourcePack.h"

namespace Crowny
{
    namespace
    {
        constexpr uint8_t PACK_MAGIC[8] = { 'C', 'W', 'P', 'A', 'C', 'K', '0', '1' };
        constexpr uint32_t PACK_VERSION = 1;
        constexpr uint32_t MAX_PACK_ENTRIES = 65536;
        constexpr uint16_t MAX_PACK_PATH_LENGTH = 4096;
        constexpr size_t PACK_HEADER_SIZE = 24;

        template <class T> bool ReadValue(const Vector<uint8_t>& data, size_t& cursor, T& value)
        {
            if (cursor > data.size() || sizeof(T) > data.size() - cursor)
                return false;
            std::memcpy(&value, data.data() + cursor, sizeof(T));
            cursor += sizeof(T);
            return true;
        }
    } // namespace

    BuiltInResourcePack::BuiltInResourcePack(const Path& packPath)
      : m_Path(packPath.lexically_normal()), m_LooseRoot(m_Path.parent_path().parent_path())
    {
        const char* forceSource = std::getenv("CROWNY_BUILTIN_SOURCE");
        m_ForceSourceFiles = forceSource != nullptr && StringView(forceSource) != "0" && !StringView(forceSource).empty();
        m_Valid = Load();
    }

    String BuiltInResourcePack::NormalizePath(const Path& path) const
    {
        Path normalizedPath = path.lexically_normal();
        if (normalizedPath.is_absolute())
        {
            const Path relative = normalizedPath.lexically_relative(m_LooseRoot);
            const String relativeString = relative.generic_string();
            if (!relative.empty() && !relativeString.starts_with(".."))
                normalizedPath = relative;
        }

        String normalized = normalizedPath.generic_string();
        while (normalized.starts_with("./"))
            normalized.erase(0, 2);
        while (!normalized.empty() && normalized.front() == '/')
            normalized.erase(normalized.begin());
        return normalized;
    }

    Path BuiltInResourcePack::GetLoosePath(const Path& path) const
    {
        return path.is_absolute() ? path : (m_LooseRoot / path).lexically_normal();
    }

    bool BuiltInResourcePack::HasLooseFile(const Path& path) const
    {
#ifdef CW_DIST
        (void)path;
        return false;
#else
        std::error_code error;
        return fs::is_regular_file(GetLoosePath(path), error) && !error;
#endif
    }

    bool BuiltInResourcePack::Load()
    {
        std::ifstream stream(m_Path, std::ios::binary | std::ios::ate);
        if (!stream)
            return false;

        const std::streamsize streamSize = stream.tellg();
        if (streamSize < static_cast<std::streamsize>(PACK_HEADER_SIZE))
            return false;

        m_Data.resize(static_cast<size_t>(streamSize));
        stream.seekg(0, std::ios::beg);
        if (!stream.read(reinterpret_cast<char*>(m_Data.data()), streamSize))
        {
            m_Data.clear();
            return false;
        }

        if (std::memcmp(m_Data.data(), PACK_MAGIC, sizeof(PACK_MAGIC)) != 0)
            return false;

        size_t cursor = sizeof(PACK_MAGIC);
        uint32_t version = 0;
        uint32_t entryCount = 0;
        uint64_t indexOffset = 0;
        if (!ReadValue(m_Data, cursor, version) || !ReadValue(m_Data, cursor, entryCount) || !ReadValue(m_Data, cursor, indexOffset) ||
            version != PACK_VERSION || entryCount > MAX_PACK_ENTRIES || indexOffset < PACK_HEADER_SIZE || indexOffset > m_Data.size())
            return false;

        cursor = static_cast<size_t>(indexOffset);
        m_Entries.reserve(entryCount);
        for (uint32_t index = 0; index < entryCount; index++)
        {
            uint16_t pathLength = 0;
            uint16_t reserved = 0;
            Entry entry;
            if (!ReadValue(m_Data, cursor, pathLength) || !ReadValue(m_Data, cursor, reserved) || !ReadValue(m_Data, cursor, entry.Offset) ||
                !ReadValue(m_Data, cursor, entry.Size) || pathLength == 0 || pathLength > MAX_PACK_PATH_LENGTH || cursor > m_Data.size() ||
                pathLength > m_Data.size() - cursor || entry.Offset < PACK_HEADER_SIZE || entry.Offset > indexOffset ||
                entry.Size > indexOffset - entry.Offset)
            {
                m_Entries.clear();
                return false;
            }

            String path(reinterpret_cast<const char*>(m_Data.data() + cursor), pathLength);
            cursor += pathLength;
            const auto [iter, inserted] = m_Entries.emplace(std::move(path), entry);
            if (!inserted)
            {
                m_Entries.clear();
                return false;
            }
        }

        std::error_code error;
        m_PackWriteTime = fs::last_write_time(m_Path, error);
        if (error)
            m_PackWriteTime = {};
        return true;
    }

    bool BuiltInResourcePack::PreferSourceFile(const Path& path) const
    {
#ifdef CW_DIST
        (void)path;
        return false;
#else
        const Path loosePath = GetLoosePath(path);
        std::error_code error;
        if (!fs::is_regular_file(loosePath, error) || error)
            return false;
        if (m_ForceSourceFiles)
            return true;
        const fs::file_time_type sourceWriteTime = fs::last_write_time(loosePath, error);
        return !error && sourceWriteTime > m_PackWriteTime;
#endif
    }

    const BuiltInResourcePack::Entry* BuiltInResourcePack::Find(const Path& path) const
    {
        if (!m_Valid)
            return nullptr;
        const String normalized = NormalizePath(path);
        const auto iter = m_Entries.find(StringView(normalized));
        return iter == m_Entries.end() ? nullptr : &iter->second;
    }

    bool BuiltInResourcePack::Contains(const Path& path) const { return Find(path) != nullptr || HasLooseFile(path); }

    bool BuiltInResourcePack::TryGetSize(const Path& path, uint64_t& size) const
    {
        const Entry* entry = Find(path);
        if (entry == nullptr)
        {
            if (!HasLooseFile(path))
                return false;
            std::error_code error;
            size = fs::file_size(GetLoosePath(path), error);
            return !error;
        }
        if (PreferSourceFile(path))
        {
            std::error_code error;
            size = fs::file_size(GetLoosePath(path), error);
            return !error;
        }
        size = entry->Size;
        return true;
    }

    Ref<DataStream> BuiltInResourcePack::Open(const Path& path) const
    {
        const Entry* entry = Find(path);
        if (entry == nullptr)
            return HasLooseFile(path) ? CreateRef<FileDataStream>(GetLoosePath(path), DataStream::READ, true) : nullptr;
        if (PreferSourceFile(path))
            return CreateRef<FileDataStream>(GetLoosePath(path), DataStream::READ, true);
        return CreateRef<MemoryDataStream>(const_cast<uint8_t*>(m_Data.data() + entry->Offset), static_cast<size_t>(entry->Size));
    }
} // namespace Crowny

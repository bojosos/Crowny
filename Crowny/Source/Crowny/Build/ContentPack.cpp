#include "cwpch.h"

#include "Crowny/Build/ContentPack.h"

#include <mbedtls/sha256.h>

#include <bit>
#include <cctype>
#include <iomanip>
#include <type_traits>

namespace Crowny
{
    namespace
    {
        constexpr uint8_t PACK_MAGIC[8] = { 'C', 'W', 'P', 'A', 'C', 'K', '0', '2' };
        constexpr uint64_t FIXED_HEADER_SIZE = 48;
        constexpr uint32_t MAX_PACK_ENTRIES = 1'000'000;
        constexpr uint16_t MAX_RUNTIME_PATH = 4096;
        constexpr uint16_t MAX_DESCRIPTOR_STRING = 1024;
        constexpr uint64_t ALIGNMENT = 16;

        template <class T> void WriteLittleEndian(std::ostream& stream, T value)
        {
            static_assert(std::is_integral_v<T>);
            using Unsigned = std::make_unsigned_t<T>;
            Unsigned bits = static_cast<Unsigned>(value);
            for (size_t index = 0; index < sizeof(T); index++)
            {
                stream.put(static_cast<char>(bits & 0xffu));
                bits >>= 8;
            }
        }

        template <class T> bool ReadLittleEndian(std::istream& stream, T& value)
        {
            static_assert(std::is_integral_v<T>);
            using Unsigned = std::make_unsigned_t<T>;
            Unsigned bits = 0;
            for (size_t index = 0; index < sizeof(T); index++)
            {
                const int byte = stream.get();
                if (byte == std::char_traits<char>::eof())
                    return false;
                bits |= static_cast<Unsigned>(static_cast<unsigned char>(byte)) << (index * 8);
            }
            if constexpr (std::is_signed_v<T>)
                value = std::bit_cast<T>(bits);
            else
                value = static_cast<T>(bits);
            return true;
        }

        String Hex(const uint8_t* bytes, size_t size)
        {
            static constexpr char DIGITS[] = "0123456789abcdef";
            String result(size * 2, '0');
            for (size_t index = 0; index < size; index++)
            {
                result[index * 2] = DIGITS[bytes[index] >> 4];
                result[index * 2 + 1] = DIGITS[bytes[index] & 0x0f];
            }
            return result;
        }

        String NormalizeRuntimePath(const Path& path) { return NormalizePortableBuildPath(path); }

        String Lowercase(String value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return value;
        }

        uint64_t Align(uint64_t value) { return (value + ALIGNMENT - 1) & ~(ALIGNMENT - 1); }

        bool GetOutputPosition(std::ostream& stream, uint64_t& position)
        {
            const std::streampos rawPosition = stream.tellp();
            if (rawPosition < 0)
                return false;
            position = static_cast<uint64_t>(rawPosition);
            return true;
        }

        bool CopyFileToStreamAndHash(const Path& source, std::ostream& output, uint64_t expectedSize, String& sha256)
        {
            std::ifstream input(source, std::ios::binary);
            if (!input)
                return false;
            mbedtls_sha256_context context;
            mbedtls_sha256_init(&context);
            if (mbedtls_sha256_starts(&context, false) != 0)
            {
                mbedtls_sha256_free(&context);
                return false;
            }
            Array<char, 64 * 1024> buffer{};
            uint64_t copied = 0;
            while (input)
            {
                input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                const std::streamsize count = input.gcount();
                if (count <= 0)
                    break;
                if (mbedtls_sha256_update(&context, reinterpret_cast<const uint8_t*>(buffer.data()), static_cast<size_t>(count)) != 0)
                {
                    mbedtls_sha256_free(&context);
                    return false;
                }
                output.write(buffer.data(), count);
                copied += static_cast<uint64_t>(count);
            }
            uint8_t digest[32]{};
            const bool complete = input.eof() && output && copied == expectedSize;
            const int status = complete ? mbedtls_sha256_finish(&context, digest) : -1;
            mbedtls_sha256_free(&context);
            if (status != 0)
                return false;
            sha256 = Hex(digest, sizeof(digest));
            return true;
        }

        String ReplaceAtomically(const Path& temporary, const Path& destination)
        {
            std::error_code error;
#ifdef CW_PLATFORM_WIN32
            const Path backup = destination.string() + ".previous";
            fs::remove(backup, error);
            error.clear();
            if (fs::exists(destination))
            {
                fs::rename(destination, backup, error);
                if (error)
                    return "Cannot replace existing pack '" + destination.string() + "': " + error.message();
            }
            fs::rename(temporary, destination, error);
            if (error)
            {
                std::error_code restoreError;
                if (fs::exists(backup))
                    fs::rename(backup, destination, restoreError);
                return "Cannot publish pack '" + destination.string() + "': " + error.message();
            }
            fs::remove(backup, error);
#else
            fs::rename(temporary, destination, error);
            if (error)
                return "Cannot publish pack '" + destination.string() + "': " + error.message();
#endif
            return {};
        }
    } // namespace

    String ComputeBytesSha256(const uint8_t* data, size_t size)
    {
        uint8_t digest[32]{};
        if (mbedtls_sha256(data, size, digest, false) != 0)
            return {};
        return Hex(digest, sizeof(digest));
    }

    String ComputeFileSha256(const Path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            return {};
        mbedtls_sha256_context context;
        mbedtls_sha256_init(&context);
        if (mbedtls_sha256_starts(&context, false) != 0)
        {
            mbedtls_sha256_free(&context);
            return {};
        }
        Array<uint8_t, 64 * 1024> buffer{};
        while (stream)
        {
            stream.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
            const std::streamsize count = stream.gcount();
            if (count > 0 && mbedtls_sha256_update(&context, buffer.data(), static_cast<size_t>(count)) != 0)
            {
                mbedtls_sha256_free(&context);
                return {};
            }
        }
        uint8_t digest[32]{};
        const int status = stream.eof() ? mbedtls_sha256_finish(&context, digest) : -1;
        mbedtls_sha256_free(&context);
        return status == 0 ? Hex(digest, sizeof(digest)) : String();
    }

    String ContentPackWriter::Write(const Path& path, const ContentPackDescriptor& descriptor, const Vector<ContentPackInput>& inputs)
    {
        if (descriptor.PackId.empty() || descriptor.PackId.size() > MAX_DESCRIPTOR_STRING || descriptor.EngineVersion.empty() ||
            descriptor.EngineVersion.size() > MAX_DESCRIPTOR_STRING)
            return "Content pack descriptor has an invalid pack ID or engine version.";
        if (inputs.size() > MAX_PACK_ENTRIES)
            return "Content pack contains too many entries.";

        Vector<ContentPackInput> sorted = inputs;
        std::sort(sorted.begin(), sorted.end(), [](const ContentPackInput& left, const ContentPackInput& right) {
            return NormalizeRuntimePath(left.LogicalPath) < NormalizeRuntimePath(right.LogicalPath);
        });
        Set<UUID> ids;
        Set<String> paths;
        Set<String> lowercasePaths;
        for (ContentPackInput& input : sorted)
        {
            if (!IsSafeRelativeBuildPath(input.LogicalPath))
                return "Content pack entry path is unsafe: '" + input.LogicalPath.string() + "'.";
            input.LogicalPath = NormalizeRuntimePath(input.LogicalPath);
            if (input.Id.Empty())
                return "Content pack entry '" + input.LogicalPath.string() + "' has an empty asset ID.";
            if (!IsSafeRelativeBuildPath(input.LogicalPath) || input.LogicalPath.generic_string().size() > MAX_RUNTIME_PATH)
                return "Content pack entry path is unsafe or too long: '" + input.LogicalPath.string() + "'.";
            if (!fs::is_regular_file(input.SourcePath))
                return "Cooked asset does not exist: '" + input.SourcePath.string() + "'.";
            if (!ids.insert(input.Id).second)
                return "Content pack contains duplicate asset ID '" + input.Id.ToString() + "'.";
            const String logicalPath = input.LogicalPath.generic_string();
            if (!paths.insert(logicalPath).second || !lowercasePaths.insert(Lowercase(logicalPath)).second)
                return "Content pack contains a duplicate or case-colliding path: '" + logicalPath + "'.";
        }

        std::error_code error;
        if (!path.parent_path().empty())
            fs::create_directories(path.parent_path(), error);
        if (error)
            return "Cannot create pack output directory: " + error.message();
        const Path temporary = path.string() + ".tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
            return "Cannot create content pack '" + temporary.string() + "'.";

        output.write(reinterpret_cast<const char*>(PACK_MAGIC), sizeof(PACK_MAGIC));
        const uint32_t version = CONTENT_PACK_FORMAT;
        const uint32_t entryCount = static_cast<uint32_t>(sorted.size());
        uint64_t indexOffset = 0;
        uint64_t indexSize = 0;
        const uint16_t packIdLength = static_cast<uint16_t>(descriptor.PackId.size());
        const uint16_t engineVersionLength = static_cast<uint16_t>(descriptor.EngineVersion.size());
        WriteLittleEndian(output, version);
        WriteLittleEndian(output, entryCount);
        WriteLittleEndian(output, indexOffset);
        WriteLittleEndian(output, indexSize);
        WriteLittleEndian(output, descriptor.PlayerAbi);
        WriteLittleEndian(output, descriptor.ContentSchema);
        WriteLittleEndian(output, descriptor.MountPriority);
        WriteLittleEndian(output, packIdLength);
        WriteLittleEndian(output, engineVersionLength);
        output.write(descriptor.PackId.data(), packIdLength);
        output.write(descriptor.EngineVersion.data(), engineVersionLength);
        uint64_t position = 0;
        if (!GetOutputPosition(output, position))
        {
            output.close();
            fs::remove(temporary);
            return "Writing content pack header failed for '" + path.string() + "'.";
        }
        const uint64_t dataOffset = Align(position);
        while (position < dataOffset)
        {
            output.put('\0');
            position++;
        }

        Vector<ContentPackEntry> entries;
        entries.reserve(sorted.size());
        for (const ContentPackInput& input : sorted)
        {
            ContentPackEntry entry;
            entry.Id = input.Id;
            entry.LogicalPath = input.LogicalPath;
            if (!GetOutputPosition(output, entry.Offset))
            {
                output.close();
                fs::remove(temporary);
                return "Writing content pack data failed for '" + path.string() + "'.";
            }
            entry.StoredSize = fs::file_size(input.SourcePath, error);
            if (error)
            {
                output.close();
                fs::remove(temporary);
                return "Cannot read cooked asset size for '" + input.SourcePath.string() + "': " + error.message();
            }
            entry.UncompressedSize = entry.StoredSize;
            if (!CopyFileToStreamAndHash(input.SourcePath, output, entry.StoredSize, entry.Sha256))
            {
                output.close();
                fs::remove(temporary);
                return "Cannot read cooked asset '" + input.SourcePath.string() + "'.";
            }
            if (!GetOutputPosition(output, position))
            {
                output.close();
                fs::remove(temporary);
                return "Writing content pack data failed for '" + path.string() + "'.";
            }
            const uint64_t aligned = Align(position);
            while (position < aligned)
            {
                output.put('\0');
                position++;
            }
            entries.push_back(std::move(entry));
        }

        if (!GetOutputPosition(output, indexOffset))
        {
            output.close();
            fs::remove(temporary);
            return "Writing content pack index failed for '" + path.string() + "'.";
        }
        for (const ContentPackEntry& entry : entries)
        {
            const String logicalPath = entry.LogicalPath.generic_string();
            const String uuid = entry.Id.ToString();
            const uint16_t pathLength = static_cast<uint16_t>(logicalPath.size());
            const uint16_t flags = 0;
            WriteLittleEndian(output, pathLength);
            WriteLittleEndian(output, flags);
            output.write(uuid.data(), static_cast<std::streamsize>(uuid.size()));
            WriteLittleEndian(output, entry.Offset);
            WriteLittleEndian(output, entry.StoredSize);
            WriteLittleEndian(output, entry.UncompressedSize);
            const Vector<uint8_t> hashBytes = [&]() {
                Vector<uint8_t> bytes(32);
                for (size_t index = 0; index < bytes.size(); index++)
                    bytes[index] = static_cast<uint8_t>(std::stoi(entry.Sha256.substr(index * 2, 2), nullptr, 16));
                return bytes;
            }();
            output.write(reinterpret_cast<const char*>(hashBytes.data()), static_cast<std::streamsize>(hashBytes.size()));
            output.write(logicalPath.data(), pathLength);
        }
        if (!GetOutputPosition(output, position) || position < indexOffset)
        {
            output.close();
            fs::remove(temporary);
            return "Writing content pack index failed for '" + path.string() + "'.";
        }
        indexSize = position - indexOffset;
        output.seekp(16, std::ios::beg);
        WriteLittleEndian(output, indexOffset);
        WriteLittleEndian(output, indexSize);
        output.flush();
        if (!output)
        {
            output.close();
            fs::remove(temporary);
            return "Writing content pack failed for '" + path.string() + "'.";
        }
        output.close();
        return ReplaceAtomically(temporary, path);
    }

    String ContentPackReader::Open(const Path& path)
    {
        std::unique_lock lock(m_Mutex);
        ResetLocked();
        const auto fail = [this](String error) {
            ResetLocked();
            return error;
        };
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream)
            return fail("Cannot open content pack '" + path.string() + "'.");
        const std::streampos fileEnd = stream.tellg();
        if (fileEnd < 0)
            return fail("Cannot determine content pack size for '" + path.string() + "'.");
        m_FileSize = static_cast<uint64_t>(fileEnd);
        if (m_FileSize < FIXED_HEADER_SIZE)
            return fail("Content pack header is truncated.");
        stream.seekg(0, std::ios::beg);
        uint8_t magic[8]{};
        stream.read(reinterpret_cast<char*>(magic), sizeof(magic));
        uint32_t version = 0;
        uint32_t entryCount = 0;
        uint64_t indexOffset = 0;
        uint64_t indexSize = 0;
        uint16_t packIdLength = 0;
        uint16_t engineVersionLength = 0;
        if (std::memcmp(magic, PACK_MAGIC, sizeof(magic)) != 0 || !ReadLittleEndian(stream, version) || !ReadLittleEndian(stream, entryCount) ||
            !ReadLittleEndian(stream, indexOffset) || !ReadLittleEndian(stream, indexSize) || !ReadLittleEndian(stream, m_Descriptor.PlayerAbi) ||
            !ReadLittleEndian(stream, m_Descriptor.ContentSchema) || !ReadLittleEndian(stream, m_Descriptor.MountPriority) ||
            !ReadLittleEndian(stream, packIdLength) || !ReadLittleEndian(stream, engineVersionLength))
            return fail("Content pack header is invalid.");
        if (version != CONTENT_PACK_FORMAT)
            return fail("Unsupported content pack format " + std::to_string(version) + ".");
        const uint64_t descriptorEnd = FIXED_HEADER_SIZE + static_cast<uint64_t>(packIdLength) + engineVersionLength;
        const uint64_t minimumIndexSize = static_cast<uint64_t>(entryCount) * 96;
        if (entryCount > MAX_PACK_ENTRIES || packIdLength == 0 || packIdLength > MAX_DESCRIPTOR_STRING || engineVersionLength == 0 ||
            engineVersionLength > MAX_DESCRIPTOR_STRING || descriptorEnd > m_FileSize || indexOffset < Align(descriptorEnd) ||
            indexOffset > m_FileSize || indexSize > m_FileSize - indexOffset || indexSize < minimumIndexSize || indexOffset + indexSize != m_FileSize)
            return fail("Content pack header contains invalid bounds.");
        m_Descriptor.PackId.resize(packIdLength);
        m_Descriptor.EngineVersion.resize(engineVersionLength);
        if (!stream.read(m_Descriptor.PackId.data(), packIdLength) || !stream.read(m_Descriptor.EngineVersion.data(), engineVersionLength))
            return fail("Content pack descriptor is truncated.");

        stream.seekg(static_cast<std::streamoff>(indexOffset), std::ios::beg);
        if (!stream)
            return fail("Content pack entry table offset is invalid.");
        const uint64_t indexEnd = indexOffset + indexSize;
        const uint64_t dataStart = Align(descriptorEnd);
        Set<String> lowercasePaths;
        Vector<Pair<uint64_t, uint64_t>> dataRanges;
        dataRanges.reserve(entryCount);
        m_Entries.reserve(entryCount);
        for (uint32_t index = 0; index < entryCount; index++)
        {
            const std::streampos rawPosition = stream.tellg();
            if (rawPosition < 0)
                return fail("Content pack entry table is truncated.");
            const uint64_t position = static_cast<uint64_t>(rawPosition);
            if (position > indexEnd || indexEnd - position < 96)
                return fail("Content pack entry table is truncated.");
            uint16_t pathLength = 0;
            uint16_t flags = 0;
            char uuidString[36]{};
            uint8_t digest[32]{};
            ContentPackEntry entry;
            if (!ReadLittleEndian(stream, pathLength) || !ReadLittleEndian(stream, flags) || !stream.read(uuidString, sizeof(uuidString)) ||
                !ReadLittleEndian(stream, entry.Offset) || !ReadLittleEndian(stream, entry.StoredSize) ||
                !ReadLittleEndian(stream, entry.UncompressedSize) || !stream.read(reinterpret_cast<char*>(digest), sizeof(digest)))
                return fail("Content pack entry table is truncated.");
            const std::streampos rawPathPosition = stream.tellg();
            if (rawPathPosition < 0)
                return fail("Content pack entry table is truncated.");
            const uint64_t pathPosition = static_cast<uint64_t>(rawPathPosition);
            if (flags != 0 || pathLength == 0 || pathLength > MAX_RUNTIME_PATH || entry.Offset < dataStart || entry.Offset % ALIGNMENT != 0 ||
                entry.Offset > indexOffset || entry.StoredSize > indexOffset - entry.Offset || entry.StoredSize != entry.UncompressedSize ||
                pathPosition > indexEnd || pathLength > indexEnd - pathPosition)
                return fail("Content pack entry contains invalid bounds or compression flags.");
            String logicalPath(pathLength, '\0');
            if (!stream.read(logicalPath.data(), pathLength))
                return fail("Content pack entry path is truncated.");
            entry.Id = UUID(String(uuidString, sizeof(uuidString)));
            entry.LogicalPath = logicalPath;
            entry.Sha256 = Hex(digest, sizeof(digest));
            const String normalizedPath = NormalizeRuntimePath(entry.LogicalPath);
            if (entry.Id.Empty() || !IsSafeRelativeBuildPath(entry.LogicalPath) || normalizedPath != logicalPath)
                return fail("Content pack entry has an invalid asset ID or path.");
            const size_t entryIndex = m_Entries.size();
            if (!m_PathIndex.emplace(normalizedPath, entryIndex).second || !lowercasePaths.emplace(Lowercase(normalizedPath)).second ||
                !m_UuidIndex.emplace(entry.Id, entryIndex).second)
                return fail("Content pack contains duplicate asset IDs or case-colliding runtime paths.");
            if (entry.StoredSize != 0)
                dataRanges.emplace_back(entry.Offset, entry.Offset + entry.StoredSize);
            m_Entries.push_back(std::move(entry));
        }
        const std::streampos rawIndexEnd = stream.tellg();
        if (rawIndexEnd < 0 || static_cast<uint64_t>(rawIndexEnd) != indexEnd)
            return fail("Content pack entry table size does not match its header.");
        std::sort(dataRanges.begin(), dataRanges.end());
        for (size_t index = 1; index < dataRanges.size(); index++)
        {
            if (dataRanges[index].first < dataRanges[index - 1].second)
                return fail("Content pack contains overlapping entry data ranges.");
        }
        m_Path = path;
        m_Open = true;
        return {};
    }

    void ContentPackReader::Close()
    {
        std::unique_lock lock(m_Mutex);
        ResetLocked();
    }

    void ContentPackReader::ResetLocked()
    {
        m_Path.clear();
        m_Descriptor = {};
        m_Entries.clear();
        m_PathIndex.clear();
        m_UuidIndex.clear();
        m_FileSize = 0;
        m_Open = false;
    }

    bool ContentPackReader::IsOpen() const
    {
        std::shared_lock lock(m_Mutex);
        return m_Open;
    }

    ContentPackDescriptor ContentPackReader::GetDescriptor() const
    {
        std::shared_lock lock(m_Mutex);
        return m_Descriptor;
    }

    Vector<ContentPackEntry> ContentPackReader::GetEntries() const
    {
        std::shared_lock lock(m_Mutex);
        return m_Entries;
    }

    std::optional<ContentPackEntry> ContentPackReader::Find(const Path& logicalPath) const
    {
        std::shared_lock lock(m_Mutex);
        if (!IsSafeRelativeBuildPath(logicalPath))
            return std::nullopt;
        const String normalized = NormalizeRuntimePath(logicalPath);
        const auto iter = m_PathIndex.find(normalized);
        return iter == m_PathIndex.end() ? std::nullopt : std::optional<ContentPackEntry>(m_Entries[iter->second]);
    }

    std::optional<ContentPackEntry> ContentPackReader::Find(const UUID& id) const
    {
        std::shared_lock lock(m_Mutex);
        const auto iter = m_UuidIndex.find(id);
        return iter == m_UuidIndex.end() ? std::nullopt : std::optional<ContentPackEntry>(m_Entries[iter->second]);
    }

    String ContentPackReader::Read(const Path& logicalPath, Vector<uint8_t>& output) const
    {
        output.clear();
        std::shared_lock lock(m_Mutex);
        if (!m_Open)
            return "Content pack is not open.";
        if (!IsSafeRelativeBuildPath(logicalPath))
            return "Content pack lookup path is unsafe: '" + logicalPath.generic_string() + "'.";
        const String normalized = NormalizeRuntimePath(logicalPath);
        const auto iter = m_PathIndex.find(normalized);
        return iter == m_PathIndex.end() ? "Content pack does not contain '" + logicalPath.generic_string() + "'."
                                         : ReadLocked(m_Entries[iter->second], output);
    }

    String ContentPackReader::Read(const UUID& id, Vector<uint8_t>& output) const
    {
        output.clear();
        std::shared_lock lock(m_Mutex);
        if (!m_Open)
            return "Content pack is not open.";
        const auto iter = m_UuidIndex.find(id);
        return iter == m_UuidIndex.end() ? "Content pack does not contain asset '" + id.ToString() + "'."
                                         : ReadLocked(m_Entries[iter->second], output);
    }

    String ContentPackReader::ReadLocked(const ContentPackEntry& entry, Vector<uint8_t>& output) const
    {
        if (entry.UncompressedSize > static_cast<uint64_t>(std::numeric_limits<size_t>::max()) ||
            entry.UncompressedSize > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max()))
            return "Content pack entry is too large for this process.";
        std::ifstream stream(m_Path, std::ios::binary);
        if (!stream)
            return "Cannot reopen content pack '" + m_Path.string() + "'.";
        output.resize(static_cast<size_t>(entry.UncompressedSize));
        stream.seekg(static_cast<std::streamoff>(entry.Offset), std::ios::beg);
        if (!output.empty() && !stream.read(reinterpret_cast<char*>(output.data()), static_cast<std::streamsize>(output.size())))
        {
            output.clear();
            return "Content pack entry is truncated: '" + entry.LogicalPath.generic_string() + "'.";
        }
        if (ComputeBytesSha256(output.data(), output.size()) != entry.Sha256)
        {
            output.clear();
            return "Content pack entry hash failed: '" + entry.LogicalPath.generic_string() + "'.";
        }
        return {};
    }
} // namespace Crowny

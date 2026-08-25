#include <catch2/catch_test_macros.hpp>

#include "Crowny/Build/ContentPack.h"

#include <atomic>
#include <fstream>
#include <limits>
#include <thread>

namespace Crowny
{
    namespace
    {
        class ContentPackTemporaryDirectory
        {
        public:
            ContentPackTemporaryDirectory()
            {
                Root = fs::temp_directory_path() / ("crowny-content-pack-tests-" + UuidGenerator::Generate().ToString());
                fs::create_directories(Root);
            }

            ~ContentPackTemporaryDirectory()
            {
                std::error_code error;
                fs::remove_all(Root, error);
            }

            Path Root;
        };

        void WriteBytes(const Path& path, const Vector<uint8_t>& bytes)
        {
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }

        void WriteText(const Path& path, StringView text)
        {
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        }

        Vector<uint8_t> ReadBytes(const Path& path)
        {
            std::ifstream stream(path, std::ios::binary | std::ios::ate);
            const std::streamsize size = stream.tellg();
            Vector<uint8_t> bytes(static_cast<size_t>(size));
            stream.seekg(0, std::ios::beg);
            stream.read(reinterpret_cast<char*>(bytes.data()), size);
            return bytes;
        }

        uint64_t ReadLittleEndian(const Vector<uint8_t>& bytes, size_t offset, size_t width)
        {
            uint64_t value = 0;
            for (size_t index = 0; index < width; index++)
                value |= static_cast<uint64_t>(bytes[offset + index]) << (index * 8);
            return value;
        }

        void WriteLittleEndian(Vector<uint8_t>& bytes, size_t offset, size_t width, uint64_t value)
        {
            for (size_t index = 0; index < width; index++)
                bytes[offset + index] = static_cast<uint8_t>(value >> (index * 8));
        }

        size_t FindBytes(const Vector<uint8_t>& bytes, StringView text)
        {
            const auto iter = std::search(bytes.begin(), bytes.end(), text.begin(), text.end());
            return iter == bytes.end() ? bytes.size() : static_cast<size_t>(std::distance(bytes.begin(), iter));
        }

        ContentPackDescriptor MakeDescriptor()
        {
            ContentPackDescriptor descriptor;
            descriptor.PackId = "security";
            descriptor.EngineVersion = "0.1.0";
            descriptor.PlayerAbi = 0x11223344;
            descriptor.ContentSchema = 0x55667788;
            descriptor.MountPriority = -2;
            return descriptor;
        }
    } // namespace

    TEST_CASE("Content pack integers use deterministic little-endian bytes", "[Build][ContentPack][Security]")
    {
        ContentPackTemporaryDirectory temporary;
        WriteText(temporary.Root / "a.asset", "alpha");
        WriteText(temporary.Root / "b.asset", "bravo");
        Vector<ContentPackInput> inputs = {
            { UUID("99999999-8888-7777-6666-555555555555"), "Assets/B.asset", temporary.Root / "b.asset" },
            { UUID("11111111-2222-3333-4444-555555555555"), "Assets/A.asset", temporary.Root / "a.asset" },
        };

        const Path first = temporary.Root / "first.cwpack";
        const Path second = temporary.Root / "second.cwpack";
        REQUIRE(ContentPackWriter::Write(first, MakeDescriptor(), inputs).empty());
        std::reverse(inputs.begin(), inputs.end());
        REQUIRE(ContentPackWriter::Write(second, MakeDescriptor(), inputs).empty());

        const Vector<uint8_t> bytes = ReadBytes(first);
        REQUIRE(bytes.size() >= 48);
        CHECK(bytes == ReadBytes(second));
        CHECK(ReadLittleEndian(bytes, 8, 4) == CONTENT_PACK_FORMAT);
        CHECK(ReadLittleEndian(bytes, 12, 4) == 2);
        CHECK(ReadLittleEndian(bytes, 32, 4) == 0x11223344);
        CHECK(ReadLittleEndian(bytes, 36, 4) == 0x55667788);
        CHECK(ReadLittleEndian(bytes, 40, 4) == 0xfffffffe);
        CHECK(ReadLittleEndian(bytes, 16, 8) < bytes.size());

        ContentPackReader reader;
        REQUIRE(reader.Open(first).empty());
        CHECK(reader.GetDescriptor().MountPriority == -2);
    }

    TEST_CASE("Content pack reader rejects truncated and out-of-range structures", "[Build][ContentPack][Security]")
    {
        ContentPackTemporaryDirectory temporary;
        WriteText(temporary.Root / "source.asset", "payload");
        const UUID id("11111111-2222-3333-4444-555555555555");
        const Path validPath = temporary.Root / "valid.cwpack";
        REQUIRE(ContentPackWriter::Write(validPath, MakeDescriptor(), { { id, "Assets/Source.asset", temporary.Root / "source.asset" } }).empty());
        const Vector<uint8_t> valid = ReadBytes(validPath);

        ContentPackReader reader;

        Vector<uint8_t> truncatedHeader(valid.begin(), valid.begin() + 47);
        WriteBytes(temporary.Root / "truncated-header.cwpack", truncatedHeader);
        CHECK_FALSE(reader.Open(temporary.Root / "truncated-header.cwpack").empty());
        CHECK_FALSE(reader.IsOpen());

        Vector<uint8_t> truncatedTable = valid;
        truncatedTable.pop_back();
        WriteBytes(temporary.Root / "truncated-table.cwpack", truncatedTable);
        CHECK_FALSE(reader.Open(temporary.Root / "truncated-table.cwpack").empty());
        CHECK_FALSE(reader.IsOpen());

        Vector<uint8_t> invalidIndex = valid;
        WriteLittleEndian(invalidIndex, 16, 8, std::numeric_limits<uint64_t>::max());
        WriteBytes(temporary.Root / "invalid-index.cwpack", invalidIndex);
        CHECK_FALSE(reader.Open(temporary.Root / "invalid-index.cwpack").empty());
        CHECK_FALSE(reader.IsOpen());

        Vector<uint8_t> invalidEntry = valid;
        const size_t indexOffset = static_cast<size_t>(ReadLittleEndian(invalidEntry, 16, 8));
        REQUIRE(indexOffset + 48 <= invalidEntry.size());
        WriteLittleEndian(invalidEntry, indexOffset + 40, 8, 0);
        WriteBytes(temporary.Root / "invalid-entry.cwpack", invalidEntry);
        CHECK_FALSE(reader.Open(temporary.Root / "invalid-entry.cwpack").empty());
        CHECK_FALSE(reader.IsOpen());
    }

    TEST_CASE("Content pack reader rejects duplicate and case-colliding paths", "[Build][ContentPack][Security]")
    {
        ContentPackTemporaryDirectory temporary;
        WriteText(temporary.Root / "a.asset", "alpha");
        WriteText(temporary.Root / "b.asset", "bravo");
        const Path packPath = temporary.Root / "case-collision.cwpack";
        REQUIRE(ContentPackWriter::Write(packPath, MakeDescriptor(),
                                         {
                                           { UUID("11111111-2222-3333-4444-555555555555"), "Assets/A.asset", temporary.Root / "a.asset" },
                                           { UUID("99999999-8888-7777-6666-555555555555"), "Assets/B.asset", temporary.Root / "b.asset" },
                                         })
                  .empty());

        const Vector<uint8_t> validBytes = ReadBytes(packPath);
        const size_t secondPath = FindBytes(validBytes, "Assets/B.asset");
        REQUIRE(secondPath != validBytes.size());

        ContentPackReader reader;
        Vector<uint8_t> duplicateBytes = validBytes;
        duplicateBytes[secondPath + 7] = 'A';
        WriteBytes(packPath, duplicateBytes);
        CHECK_FALSE(reader.Open(packPath).empty());
        CHECK_FALSE(reader.IsOpen());

        Vector<uint8_t> caseCollisionBytes = validBytes;
        caseCollisionBytes[secondPath + 7] = 'a';
        WriteBytes(packPath, caseCollisionBytes);
        CHECK_FALSE(reader.Open(packPath).empty());
        CHECK_FALSE(reader.IsOpen());
    }

    TEST_CASE("Content pack paths are portable and traversal lookups fail", "[Build][ContentPack][Security]")
    {
        ContentPackTemporaryDirectory temporary;
        WriteText(temporary.Root / "source.asset", "payload");
        const UUID id("11111111-2222-3333-4444-555555555555");
        const Path packPath = temporary.Root / "portable.cwpack";
        REQUIRE(
          ContentPackWriter::Write(packPath, MakeDescriptor(), { { id, "Assets\\Folder\\Source.asset", temporary.Root / "source.asset" } }).empty());
        CHECK_FALSE(ContentPackWriter::Write(temporary.Root / "traversal.cwpack", MakeDescriptor(),
                                             { { id, "Assets/Folder/../Source.asset", temporary.Root / "source.asset" } })
                      .empty());

        ContentPackReader reader;
        REQUIRE(reader.Open(packPath).empty());
        REQUIRE(reader.Find("Assets/Folder/Source.asset").has_value());
        CHECK_FALSE(reader.Find("Assets/Folder/../Folder/Source.asset").has_value());
        Vector<uint8_t> payload;
        CHECK_FALSE(reader.Read("Assets/Folder/../Folder/Source.asset", payload).empty());
        CHECK(payload.empty());
    }

    TEST_CASE("Content pack entries survive lookup and allow concurrent reads", "[Build][ContentPack][Security]")
    {
        ContentPackTemporaryDirectory temporary;
        const String payload(256 * 1024, 'p');
        WriteText(temporary.Root / "source.asset", payload);
        const UUID id("11111111-2222-3333-4444-555555555555");
        const Path packPath = temporary.Root / "concurrent.cwpack";
        REQUIRE(ContentPackWriter::Write(packPath, MakeDescriptor(), { { id, "Assets/Source.asset", temporary.Root / "source.asset" } }).empty());

        ContentPackReader reader;
        REQUIRE(reader.Open(packPath).empty());
        const std::optional<ContentPackEntry> retainedEntry = reader.Find(id);
        REQUIRE(retainedEntry.has_value());
        reader.Close();
        CHECK(retainedEntry->LogicalPath == Path("Assets/Source.asset"));
        REQUIRE(reader.Open(packPath).empty());

        std::atomic<bool> start = false;
        std::atomic<uint32_t> failures = 0;
        Vector<Thread> readers;
        for (uint32_t threadIndex = 0; threadIndex < 8; threadIndex++)
        {
            readers.emplace_back([&]() {
                while (!start.load(std::memory_order_acquire))
                    std::this_thread::yield();
                for (uint32_t iteration = 0; iteration < 16; iteration++)
                {
                    Vector<uint8_t> bytes;
                    if (!reader.Read(id, bytes).empty() || bytes.size() != payload.size() || bytes.front() != 'p' || bytes.back() != 'p')
                        failures.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        start.store(true, std::memory_order_release);
        for (Thread& thread : readers)
            thread.join();
        CHECK(failures.load() == 0);

        std::atomic<bool> closeRaceStart = false;
        failures.store(0);
        readers.clear();
        for (uint32_t threadIndex = 0; threadIndex < 8; threadIndex++)
        {
            readers.emplace_back([&]() {
                while (!closeRaceStart.load(std::memory_order_acquire))
                    std::this_thread::yield();
                Vector<uint8_t> bytes;
                const String error = reader.Read(id, bytes);
                if (!error.empty() && error != "Content pack is not open.")
                    failures.fetch_add(1, std::memory_order_relaxed);
            });
        }
        closeRaceStart.store(true, std::memory_order_release);
        reader.Close();
        for (Thread& thread : readers)
            thread.join();
        CHECK(failures.load() == 0);
        CHECK_FALSE(reader.IsOpen());
    }

    TEST_CASE("Content pack payload hashes detect data corruption", "[Build][ContentPack][Security]")
    {
        ContentPackTemporaryDirectory temporary;
        WriteText(temporary.Root / "source.asset", "payload");
        const Path packPath = temporary.Root / "corrupt-payload.cwpack";
        REQUIRE(ContentPackWriter::Write(packPath, MakeDescriptor(),
                                         { { UUID("11111111-2222-3333-4444-555555555555"), "Assets/Source.asset", temporary.Root / "source.asset" } })
                  .empty());

        Vector<uint8_t> bytes = ReadBytes(packPath);
        REQUIRE(bytes.size() > 64);
        bytes[64] ^= 0xff;
        WriteBytes(packPath, bytes);

        ContentPackReader reader;
        REQUIRE(reader.Open(packPath).empty());
        Vector<uint8_t> payload;
        CHECK_FALSE(reader.Read("Assets/Source.asset", payload).empty());
        CHECK(payload.empty());
    }
} // namespace Crowny

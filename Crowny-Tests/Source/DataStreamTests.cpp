#include "Crowny/Common/DataStream.h"
#include "Crowny/Memory/AllocationCounter.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

using namespace Crowny;

TEST_CASE("MemoryDataStream appends serialized fields without quadratic allocation", "[DataStream]")
{
    MemoryDataStream stream(4096);
    constexpr uint32_t fieldCount = 16385;
    const auto before = Memory::GetThreadAllocationSnapshot();
    for (uint32_t field = 0; field < fieldCount; field++)
        stream.Write(&field, sizeof(field));
    const auto allocations = Memory::GetThreadAllocationDelta(before, Memory::GetThreadAllocationSnapshot());
    const size_t byteCount = fieldCount * sizeof(uint32_t);
    CHECK(allocations.AllocationCount < 32);
    CHECK(allocations.RequestedBytes < byteCount * 4);
    CHECK(stream.Size() == byteCount);
    CHECK(stream.Tell() == byteCount);
    stream.Seek(0);
    for (uint32_t field = 0; field < fieldCount; field++)
    {
        uint32_t actual = 0;
        REQUIRE(stream.Read(&actual, sizeof(actual)) == sizeof(actual));
        REQUIRE(actual == field);
    }
    CHECK(stream.Eof());
    uint8_t extra = 0;
    CHECK(stream.Read(&extra, 1) == 0);
}

TEST_CASE("MemoryDataStream::Basic", "[DataStream]")
{
    SECTION("Read and Write")
    {
        MemoryDataStream stream(10);
        uint32_t data = 0x12345678;
        stream.Write(&data, sizeof(data));

        CHECK(stream.Tell() == sizeof(data));
        CHECK(stream.Size() == 10);

        stream.Seek(0);
        uint32_t readData = 0;
        stream.Read(&readData, sizeof(readData));
        CHECK(readData == data);
    }

    SECTION("Skip and Seek")
    {
        uint8_t buffer[] = { 0, 1, 2, 3, 4, 5 };
        MemoryDataStream stream(buffer, sizeof(buffer));

        stream.Skip(2);
        CHECK(stream.Tell() == 2);

        uint8_t val;
        stream.Read(&val, 1);
        CHECK(val == 2);

        stream.Seek(5);
        stream.Read(&val, 1);
        CHECK(val == 5);
        CHECK(stream.Eof());
    }
}

TEST_CASE("MemoryDataStream preserves its extent when copying and moving reserved storage", "[DataStream]")
{
    MemoryDataStream source(4);
    const uint32_t first = 42;
    const uint8_t last = 7;
    source.Write(&first, sizeof(first));
    source.Write(&last, sizeof(last));
    REQUIRE(source.Size() == 5);

    SECTION("copy construction")
    {
        source.Seek(0);
        MemoryDataStream copy(source);
        CHECK(copy.Size() == 5);
        CHECK(copy.ReadAll() == Vector<uint8_t>(source.Data(), source.Data() + 5));
    }
    SECTION("copy assignment")
    {
        MemoryDataStream copy(1);
        copy = source;
        CHECK(copy.Size() == 5);
        CHECK(copy.Tell() == 5);
        copy.Write(&first, sizeof(first));
        CHECK(copy.Size() == 9);
        CHECK(source.Size() == 5);
    }
    SECTION("move construction and assignment")
    {
        MemoryDataStream moved(std::move(source));
        CHECK(moved.Size() == 5);
        moved.Write(&first, sizeof(first));
        MemoryDataStream assigned;
        assigned = std::move(moved);
        CHECK(assigned.Size() == 9);
        assigned.Write(&last, sizeof(last));
        CHECK(assigned.Size() == 10);
        assigned.Seek(0);
        uint32_t read = 0;
        assigned.Read(&read, sizeof(read));
        CHECK(read == first);
    }
    SECTION("borrowed storage cannot grow")
    {
        uint8_t storage[5] = {};
        MemoryDataStream borrowed(storage, sizeof(storage));
        CHECK(borrowed.Write(source.Data(), 5) == 5);
        CHECK(borrowed.Write(&last, sizeof(last)) == 0);
        CHECK(borrowed.Size() == 5);
    }
}

TEST_CASE("FileDataStream::Basic", "[DataStream]")
{
    fs::path testFile = "test_stream.bin";
    {
        std::ofstream fout(testFile, std::ios::binary);
        uint32_t data = 0xDEADBEEF;
        fout.write((char*)&data, sizeof(data));
    }

    {
        FileDataStream stream(testFile, DataStream::READ);
        CHECK(stream.Size() == 4);

        uint32_t readData = 0;
        stream.Read(&readData, 4);
        CHECK(readData == 0xDEADBEEF);
        CHECK(stream.Eof());
    }

    fs::remove(testFile);
}

TEST_CASE("MemoryDataStream::ReadAll", "[DataStream]")
{
    SECTION("ReadAll returns all data")
    {
        uint8_t buffer[] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE };
        MemoryDataStream stream(buffer, sizeof(buffer));

        Vector<uint8_t> result = stream.ReadAll();
        REQUIRE(result.size() == sizeof(buffer));
        CHECK(result[0] == 0xAA);
        CHECK(result[1] == 0xBB);
        CHECK(result[2] == 0xCC);
        CHECK(result[3] == 0xDD);
        CHECK(result[4] == 0xEE);
    }

    SECTION("ReadAll on empty stream returns empty vector")
    {
        MemoryDataStream stream(0);

        Vector<uint8_t> result = stream.ReadAll();
        CHECK(result.empty());
    }

    SECTION("ReadAll returns correct data after Write")
    {
        MemoryDataStream stream(8);
        uint32_t val1 = 0x12345678;
        uint32_t val2 = 0xDEADBEEF;
        stream.Write(&val1, sizeof(val1));
        stream.Write(&val2, sizeof(val2));

        stream.Seek(0);
        Vector<uint8_t> result = stream.ReadAll();
        REQUIRE(result.size() == 8);

        uint32_t read1, read2;
        std::memcpy(&read1, result.data(), sizeof(read1));
        std::memcpy(&read2, result.data() + 4, sizeof(read2));
        CHECK(read1 == 0x12345678);
        CHECK(read2 == 0xDEADBEEF);
    }
}

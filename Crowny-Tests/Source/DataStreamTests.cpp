#include <catch2/catch_test_macros.hpp>
#include "Crowny/Common/DataStream.h"
#include <filesystem>
#include <fstream>

using namespace Crowny;

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

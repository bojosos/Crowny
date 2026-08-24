#include "Crowny/Common/DataStream.h"
#include "Crowny/Utils/Compression.h"

#include <catch2/catch_test_macros.hpp>

using namespace Crowny;

TEST_CASE("FastLZ buffer compression", "[Compression]")
{
    Vector<uint8_t> source(4096);
    for (size_t i = 0; i < source.size(); i++)
        source[i] = static_cast<uint8_t>((i / 32) % 8);

    Vector<uint8_t> compressed;
    REQUIRE(Compression::Compress(compressed, source));
    CHECK(compressed.size() < source.size());

    Vector<uint8_t> decompressed;
    REQUIRE(Compression::Decompress(decompressed, compressed, source.size()));
    CHECK(decompressed == source);

    SECTION("Better-ratio level")
    {
        REQUIRE(Compression::Compress(compressed, source, CompressionMethod::FastLZ, FastLZLevel::BetterRatio));
        REQUIRE(Compression::Decompress(decompressed, compressed, source.size()));
        CHECK(decompressed == source);
    }

    SECTION("Short inputs are stored verbatim")
    {
        source = { 1, 2, 3, 4, 5, 6, 7 };
        REQUIRE(Compression::Compress(compressed, source));
        CHECK(compressed == source);
        REQUIRE(Compression::Decompress(decompressed, compressed, source.size()));
        CHECK(decompressed == source);
    }

    SECTION("Empty inputs round trip")
    {
        source.clear();
        REQUIRE(Compression::Compress(compressed, source));
        CHECK(compressed.empty());
        REQUIRE(Compression::Decompress(decompressed, compressed, 0));
        CHECK(decompressed.empty());
    }

    SECTION("Destination bounds are checked")
    {
        Vector<uint8_t> tooSmall(Compression::CompressBound(source.size()) - 1);
        CHECK(Compression::Compress(tooSmall.data(), tooSmall.size(), source.data(), source.size()) == Compression::Error);
    }

    SECTION("Corrupt data is rejected")
    {
        const Vector<uint8_t> corrupt = { 0xE0 };
        CHECK_FALSE(Compression::Decompress(decompressed, corrupt, 128));
        CHECK(decompressed.empty());
    }
}

TEST_CASE("FastLZ stream compression", "[Compression][DataStream]")
{
    Vector<uint8_t> sourceData(150000);
    for (size_t i = 0; i < sourceData.size(); i++)
        sourceData[i] = static_cast<uint8_t>((i * 17 + i / 97) & 0xFF);

    const Ref<MemoryDataStream> source = CreateRef<MemoryDataStream>();
    REQUIRE(source->Write(sourceData.data(), sourceData.size()) == sourceData.size());
    source->Seek(0);

    const Ref<MemoryDataStream> compressed = Compression::Compress(source, CompressionMethod::FastLZ, FastLZLevel::BetterRatio);
    REQUIRE(compressed);

    const Ref<MemoryDataStream> decompressed = Compression::Decompress(compressed);
    REQUIRE(decompressed);
    CHECK(decompressed->ReadAll() == sourceData);

    SECTION("Invalid frame is rejected")
    {
        uint8_t invalidHeader[8] = {};
        const Ref<MemoryDataStream> invalid = CreateRef<MemoryDataStream>(invalidHeader, sizeof(invalidHeader));
        CHECK_FALSE(Compression::Decompress(invalid));
    }
}

#include <catch2/catch_test_macros.hpp>
#include "Crowny/Common/UTF8.h"
#include <iomanip>
#include <sstream>

using namespace Crowny;

std::string ToHex(const std::string& s)
{
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char c : s)
        ss << std::setw(2) << (int)c << " ";
    return ss.str();
}

TEST_CASE("UTF8::ConversionDetailed", "[UTF8]")
{
    SECTION("Single ASCII")
    {
        String utf8 = UTF8::FromWide(L"H");
        CHECK(utf8.length() == 1);
        CHECK(utf8 == "H");
    }

    SECTION("Single 3-byte char")
    {
        String utf8 = UTF8::FromWide(L"世");
        CHECK(utf8.length() == 3);
        // U+4E16 -> 0xE4 0xB8 0x96
        CHECK((uint8_t)utf8[0] == 0xE4);
        CHECK((uint8_t)utf8[1] == 0xB8);
        CHECK((uint8_t)utf8[2] == 0x96);
    }

    SECTION("Single 4-byte char (surrogate)")
    {
        String utf8 = UTF8::FromWide(L"🌍");
        CHECK(utf8.length() == 4);
        // U+1F30D -> 0xF0 0x9F 0x8C 0x8D
        CHECK((uint8_t)utf8[0] == 0xF0);
        CHECK((uint8_t)utf8[1] == 0x9F);
        CHECK((uint8_t)utf8[2] == 0x8C);
        CHECK((uint8_t)utf8[3] == 0x8D);
    }

    SECTION("The full string")
    {
        std::wstring input = L"Hello, 世界! 🌍";
        String utf8 = UTF8::FromWide(input);
        INFO("Hex: " << ToHex(utf8));
        CHECK(utf8.length() == 19);
        
        std::wstring back = UTF8::ToWide(utf8);
        CHECK(back == input);
    }
}

TEST_CASE("UTF8 decodes Unicode code points without allocations", "[UTF8]")
{
    const String input = String("A") + "\xE4\xB8\x96" + "\xF0\x9F\x8C\x8D";
    size_t offset = 0;
    char32_t codePoint = 0;

    REQUIRE(UTF8::NextCodePoint(input, offset, codePoint));
    CHECK(codePoint == U'A');
    REQUIRE(UTF8::NextCodePoint(input, offset, codePoint));
    CHECK(codePoint == 0x4E16);
    REQUIRE(UTF8::NextCodePoint(input, offset, codePoint));
    CHECK(codePoint == 0x1F30D);
    CHECK_FALSE(UTF8::NextCodePoint(input, offset, codePoint));

    const U32String decoded = UTF8::ToUTF32(input);
    REQUIRE(decoded.size() == 3);
    CHECK(decoded[0] == U'A');
    CHECK(decoded[1] == 0x4E16);
    CHECK(decoded[2] == 0x1F30D);
}

TEST_CASE("UTF8 replaces malformed sequences and keeps decoding", "[UTF8]")
{
    const String malformed = "\xF0\x28\x8C\x28";
    const U32String decoded = UTF8::ToUTF32(malformed);

    REQUIRE(decoded.size() == 4);
    CHECK(decoded[0] == 0xFFFD);
    CHECK(decoded[1] == U'(');
    CHECK(decoded[2] == 0xFFFD);
    CHECK(decoded[3] == U'(');

    CHECK((UTF8::ToUTF32("\xC0\xAF") == U32String{ 0xFFFD, 0xFFFD }));
    CHECK((UTF8::ToUTF32("\xED\xA0\x80") == U32String{ 0xFFFD, 0xFFFD, 0xFFFD }));
    CHECK((UTF8::ToUTF32("\xF4\x90\x80\x80") == U32String{ 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD }));
}

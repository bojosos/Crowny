#include "Crowny/Common/HashedString.h"
#include "Crowny/Common/StringID.h"
#include <catch2/catch_all.hpp>

using namespace Crowny;
using namespace Crowny::Literals;

static_assert(""_hstr.GetHash() == 0x9ae16a3b2f90404full);
static_assert("hello"_hstr.GetHash() == 0xb48be5a931380ce8ull);
static_assert(sizeof(StringID) == sizeof(uint32_t));

TEST_CASE("HashedString", "[Common]")
{
    constexpr HashedString empty = ""_hstr;
    constexpr HashedString hello = "hello"_hstr;
    constexpr HashedString world = "world"_hstr;

    static_assert(empty.IsEmpty());
    static_assert(hello.GetSize() == 5);
    static_assert(hello.GetView() == "hello");
    static_assert(hello != world);

    const String owned = "hello";
    const HashedString runtime{ StringView(owned) };
    CHECK(runtime == hello);
    CHECK(runtime.Data() != hello.Data());
    CHECK(std::hash<HashedString>{}(runtime) == std::hash<HashedString>{}(hello));

    std::unordered_map<String, int, StringHash, StringEqual> values;
    values.emplace("hello", 1);
    CHECK(values.find(StringView("hello")) != values.end());
    CHECK(values.find("hello") != values.end());
    CHECK(values.find("hello"_hstr) != values.end());
    CHECK(values.find("world"_hstr) == values.end());
}

TEST_CASE("CityHash64 matches the upstream reference vectors", "[Common]")
{
    constexpr size_t maxInputSize = 65 * 65 + 65;
    Array<char, maxInputSize> data{};
    uint64_t a = 9;
    uint64_t b = 777;
    for (size_t i = 0; i < data.size(); i++)
    {
        a += b;
        b += a;
        a = (a ^ (a >> 41)) * Hashing::Detail::K0;
        b = (b ^ (b >> 41)) * Hashing::Detail::K0 + i;
        data[i] = static_cast<char>(static_cast<uint8_t>(b >> 37));
    }

    struct ReferenceVector
    {
        size_t Size;
        uint64_t Expected;
    };
    constexpr Array<ReferenceVector, 10> vectors = { {
      { 0, 0x9ae16a3b2f90404full },
      { 1, 0x541150e87f415e96ull },
      { 2, 0x0f3786a4b25827c1ull },
      { 3, 0xef923a7a1af78eabull },
      { 16, 0x03ead5f21d344056ull },
      { 17, 0x6abbfde37ee03b5bull },
      { 32, 0x0782fa1b08b475e7ull },
      { 33, 0xc5dc19b876d37a80ull },
      { 64, 0xe88419922b87176full },
      { 65, 0x105191e0ec8f7f60ull },
    } };

    for (const ReferenceVector& vector : vectors)
        CHECK(Hashing::CityHash64(data.data() + vector.Size * vector.Size, vector.Size) == vector.Expected);

    const String embeddedNull("city\0hash", 9);
    CHECK(Hashing::CityHash64(embeddedNull) == StringHash{}(embeddedNull));
    CHECK(Hash(embeddedNull) == StringHash{}(embeddedNull));

    UnorderedMap<String, int> values;
    values.emplace(embeddedNull, 42);
    CHECK(values.at(embeddedNull) == 42);
}

struct StringIDFixture
{
    StringIDFixture()
    {
        if (!StringIDTable::IsStartedUp())
        {
            StringIDTable::StartUp();
            m_OwnsTable = true;
        }
    }

    ~StringIDFixture()
    {
        if (m_OwnsTable)
            StringIDTable::Shutdown();
    }

private:
    bool m_OwnsTable = false;
};

TEST_CASE("StringID process-lifetime storage", "[Common]")
{
    const bool ownsTable = !StringIDTable::IsStartedUp();
    if (ownsTable)
        StringIDTable::StartUp();

    const StringID beforeRestart = "StringIDProcessLifetime"_sid;
    const char* stablePtr = beforeRestart.c_str();

    if (ownsTable)
    {
        StringIDTable::Shutdown();
        const StringID createdWhileStopped = "StringIDWithoutModuleWrapper"_sid;
        CHECK("StringIDProcessLifetime"_sid == beforeRestart);
        CHECK(beforeRestart.c_str() == stablePtr);
        CHECK(StringView(createdWhileStopped.c_str()) == "StringIDWithoutModuleWrapper");

        StringIDTable::StartUp();
        CHECK(StringID("StringIDProcessLifetime") == beforeRestart);
        CHECK(StringID("StringIDWithoutModuleWrapper") == createdWhileStopped);
        CHECK(beforeRestart.c_str() == stablePtr);
        StringIDTable::Shutdown();
    }
    else
    {
        CHECK("StringIDProcessLifetime"_sid == beforeRestart);
        CHECK(beforeRestart.c_str() == stablePtr);
    }
}

TEST_CASE_METHOD(StringIDFixture, "StringID", "[Common]")
{
    SECTION("Basic construction")
    {
        StringID empty;
        CHECK(empty.IsEmpty());
        CHECK(std::string(empty.c_str()) == "");

        StringID id1("TestString");
        CHECK(!id1.IsEmpty());
        CHECK(std::string(id1.c_str()) == "TestString");

        StringID id2(String("AnotherString"));
        CHECK(!id2.IsEmpty());
        CHECK(std::string(id2.c_str()) == "AnotherString");
    }

    SECTION("Interning")
    {
        StringID id1("HelloWorld");
        StringID id2("HelloWorld");
        StringID id3("Different");

        CHECK(id1 == id2);
        CHECK(id1 != id3);

        // Fast comparison (integer level)
        CHECK(id1.operator==(id2));
    }

    SECTION("Comparison with strings")
    {
        StringID id("Apple");

        CHECK(id == "Apple");
        CHECK(id != "Orange");
        CHECK("Apple" == std::string(id.c_str()));
    }

    SECTION("Empty strings")
    {
        StringID id1("");
        StringID id2(nullptr);
        StringID id3;

        CHECK(id1.IsEmpty());
        CHECK(id2.IsEmpty());
        CHECK(id3.IsEmpty());
        CHECK(id1 == id2);
        CHECK(id1 == id3);
    }

    SECTION("Hashing")
    {
        StringID id1("HashMe");
        StringID id2("HashMe");
        StringID id3("DontHashMe");

        std::hash<StringID> hasher;
        CHECK(hasher(id1) == hasher(id2));
        CHECK(hasher(id1) != hasher(id3));
    }

    SECTION("Ordering")
    {
        StringID id1("A");
        StringID id2("B");

        // Order is based on interning order, not lexicographical
        // But it should be consistent for map usage
        CHECK((id1 < id2 || id2 < id1 || id1 == id2));
    }

    SECTION("Container compatibility")
    {
        std::unordered_map<StringID, int> map;
        StringID id1("One");
        StringID id2("Two");

        map[id1] = 1;
        map[id2] = 2;

        CHECK(map[StringID("One")] == 1);
        CHECK(map.size() == 2);

        std::map<StringID, int> sortedMap;
        sortedMap[id1] = 1;
        sortedMap[id2] = 2;
        CHECK(sortedMap.size() == 2);
    }

    SECTION("Stable string storage")
    {
        const StringID stable("StablePointer");
        const char* stablePtr = stable.c_str();

        for (uint32_t i = 0; i < 4096; i++)
        {
            const String value = "StringID-Stability-" + std::to_string(i);
            StringID inserted(value);
            CHECK_FALSE(inserted.IsEmpty());
        }

        CHECK(stable.c_str() == stablePtr);
        CHECK(StringView(stablePtr) == "StablePointer");
    }

    SECTION("Repeated operations do not grow the intern table")
    {
        const StringID repeated("RepeatedIdentity");
        const size_t entryCount = StringIDTable::GetEntryCount();

        for (uint32_t i = 0; i < 4096; i++)
        {
            const StringID same("RepeatedIdentity");
            CHECK(same == repeated);
            CHECK(same.c_str() == repeated.c_str());
        }

        CHECK(StringIDTable::GetEntryCount() == entryCount);
    }

    SECTION("Cached literal")
    {
        const size_t before = StringIDTable::GetEntryCount();
        const StringID first = "CachedStringIDLiteral"_sid;
        const size_t afterFirst = StringIDTable::GetEntryCount();

        for (uint32_t i = 0; i < 4096; i++)
            CHECK("CachedStringIDLiteral"_sid == first);

        CHECK(afterFirst >= before);
        CHECK(afterFirst <= before + 1);
        CHECK(StringIDTable::GetEntryCount() == afterFirst);
    }
}

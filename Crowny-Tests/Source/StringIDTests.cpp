#include "Crowny/Common/StringID.h"
#include <catch2/catch_all.hpp>

using namespace Crowny;

TEST_CASE("StringID", "[Common]")
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
}

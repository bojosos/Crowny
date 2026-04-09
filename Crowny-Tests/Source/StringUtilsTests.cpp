#include <catch2/catch_test_macros.hpp>
#include "Crowny/Common/StringUtils.h"

using namespace Crowny;

TEST_CASE("StringUtils::SplitString", "[Utils]")
{
    SECTION("Split by comma")
    {
        String s = "a,b,c";
        auto parts = StringUtils::SplitString(s, ",");
        REQUIRE(parts.size() == 3);
        CHECK(parts[0] == "a");
        CHECK(parts[1] == "b");
        CHECK(parts[2] == "c");
    }

    SECTION("Split by multi-character separator")
    {
        String s = "a--b--c";
        auto parts = StringUtils::SplitString(s, "--");
        REQUIRE(parts.size() == 3);
        CHECK(parts[0] == "a");
        CHECK(parts[1] == "b");
        CHECK(parts[2] == "c");
    }

    SECTION("Split empty string")
    {
        String s = "";
        auto parts = StringUtils::SplitString(s, ",");
        CHECK(parts.empty());
    }
}

TEST_CASE("StringUtils::Replace", "[Utils]")
{
    String s = "hello world";
    CHECK(StringUtils::Replace(s, "world", "crowny") == "hello crowny");
    CHECK(StringUtils::Replace(s, "l", "x") == "hexxo worxd");
}

TEST_CASE("StringUtils::Parsing", "[Utils]")
{
    CHECK(StringUtils::ParseInt("123") == 123);
    CHECK(StringUtils::ParseInt("-456") == -456);
    CHECK(StringUtils::ParseFloat("1.23") == 1.23f);
    CHECK(StringUtils::ParseDouble("1.23456789") == 1.23456789);
}

TEST_CASE("StringUtils::CaseConversion", "[Utils]")
{
    String s = "Hello World";
    
    SECTION("ToLower")
    {
        String lower = s;
        StringUtils::ToLower(lower);
        CHECK(lower == "hello world");
    }

    SECTION("ToUpper")
    {
        String upper = s;
        StringUtils::ToUpper(upper);
        CHECK(upper == "HELLO WORLD");
    }
}

TEST_CASE("StringUtils::Search", "[Utils]")
{
    CHECK(StringUtils::IsSearchMathing("My_Test_Item", "test"));
    CHECK(StringUtils::IsSearchMathing("My_Test_Item", "TEST"));
    CHECK(!StringUtils::IsSearchMathing("My_Test_Item", "TEST", true)); // Case sensitive
    CHECK(StringUtils::IsSearchMathing("My_Test_Item", "TESTITEM", false, true, true)); // Strip underscores/spaces
}

TEST_CASE("StringUtils::Compare", "[Utils]")
{
    CHECK(StringUtils::CaseInsensitiveCompare("abc", "ABC"));
    CHECK(!StringUtils::CaseInsensitiveCompare("abc", "ABD"));
}

TEST_CASE("StringUtils::Miscellaneous", "[Utils]")
{
    SECTION("EndsWith")
    {
        CHECK(StringUtils::EndsWith("Hello World", "World"));
        CHECK(!StringUtils::EndsWith("Hello World", "Hello"));
    }
}

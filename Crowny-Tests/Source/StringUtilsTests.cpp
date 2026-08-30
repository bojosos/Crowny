#include "Crowny/Common/StringUtils.h"
#include "Crowny/Memory/AllocationCounter.h"

#include <catch2/catch_test_macros.hpp>

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
    CHECK(StringUtils::IsSearchMathing("", ""));
    CHECK(StringUtils::IsSearchMathing("anything", ""));
    CHECK_FALSE(StringUtils::IsSearchMathing("", "anything"));
    CHECK(StringUtils::IsSearchMathing("My_Test_Item", "test"));
    CHECK(StringUtils::IsSearchMathing("My_Test_Item", "TEST"));
    CHECK(!StringUtils::IsSearchMathing("My_Test_Item", "TEST", true)); // Case sensitive
    CHECK(StringUtils::IsSearchMathing("My_Test_Item", "TESTITEM", false, true, true)); // Strip underscores/spaces
    CHECK(StringUtils::IsSearchMathing("Alpha_Beta Gamma", "  gamma   alpha beta  ", false, false, true));
    CHECK_FALSE(StringUtils::IsSearchMathing("Alpha_Beta Gamma", "  gamma   missing  ", false, false, true));
    CHECK_FALSE(StringUtils::IsSearchMathing("Alpha Beta", "   "));
    CHECK(StringUtils::IsSearchMathing("   ", "   ", false, true));
    CHECK(StringUtils::IsSearchMathing("Alpha___Beta", "alpha beta", false, true, true));
    CHECK_FALSE(StringUtils::IsSearchMathing("Alpha___Beta", "alpha beta", false, true, false));
}

TEST_CASE("StringUtils search matching allocates nothing for long visible lists", "[Utils][Memory][Frame]")
{
    const String item = "Prefix_" + String(128u, 'x') + "_Alpha_Beta_Suffix";
    const String multiTermQuery = "  prefix   alpha beta suffix  ";
    const String compactQuery = "alpha beta";
    constexpr size_t iterationCounts[] = { 1u, 1000u, 10000u };

    for (const size_t iterationCount : iterationCounts)
    {
        bool matches = true;
        uint64_t checksum = 0u;
        const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
        for (size_t iteration = 0; iteration < iterationCount; ++iteration)
        {
            matches &= StringUtils::IsSearchMathing(item, multiTermQuery, false, false, true);
            matches &= StringUtils::IsSearchMathing(item, compactQuery, false, true, true);
            checksum += static_cast<uint8_t>(item[iteration % item.size()]);
        }
        const Memory::ThreadAllocationSnapshot delta =
          Memory::GetThreadAllocationDelta(before, Memory::GetThreadAllocationSnapshot());

        INFO("Iterations: " << iterationCount);
        CHECK(matches);
        CHECK(checksum != 0u);
        CHECK(delta.AllocationCount == 0u);
        CHECK(delta.RequestedBytes == 0u);
    }
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

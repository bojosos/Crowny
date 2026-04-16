#include <catch2/catch_test_macros.hpp>

#include "cwpch.h"
#include "Crowny/Common/Flags.h"

using namespace Crowny;

enum class TestEnum : uint32_t
{
    None = 0,
    Bit0 = 1 << 0,
    Bit1 = 1 << 1,
    Bit2 = 1 << 2,
    Bit3 = 1 << 3
};

CW_FLAGS_OPERATORS(TestEnum);

TEST_CASE("Flags Template", "[Common][Flags]")
{
    SECTION("Initialization")
    {
        Flags<TestEnum> f1;
        CHECK(f1 == TestEnum::None);
        CHECK_FALSE((bool)f1);

        Flags<TestEnum> f2(TestEnum::Bit0);
        CHECK(f2.IsSet(TestEnum::Bit0));
        CHECK(f2 == TestEnum::Bit0);
        CHECK((bool)f2);
    }

    SECTION("Bitwise Operations")
    {
        Flags<TestEnum> f = TestEnum::Bit0 | TestEnum::Bit1;
        
        CHECK(f.IsSet(TestEnum::Bit0));
        CHECK(f.IsSet(TestEnum::Bit1));
        CHECK_FALSE(f.IsSet(TestEnum::Bit2));

        CHECK(f.IsSetAny(TestEnum::Bit1 | TestEnum::Bit2));
        CHECK_FALSE(f.IsSetAny(TestEnum::Bit2 | TestEnum::Bit3));
    }

    SECTION("Set and Unset")
    {
        Flags<TestEnum> f;
        f.Set(TestEnum::Bit2);
        CHECK(f.IsSet(TestEnum::Bit2));

        f.Unset(TestEnum::Bit2);
        CHECK_FALSE(f.IsSet(TestEnum::Bit2));
        CHECK(f == TestEnum::None);
    }

    SECTION("Operators")
    {
        Flags<TestEnum> f = TestEnum::Bit0;
        f |= TestEnum::Bit1;
        CHECK(f == (TestEnum::Bit0 | TestEnum::Bit1));

        f &= TestEnum::Bit1;
        CHECK(f == TestEnum::Bit1);
        
        Flags<TestEnum> f2 = TestEnum::Bit0 | TestEnum::Bit1;
        f2 &= Flags<TestEnum>(TestEnum::Bit0);
        CHECK(f2 == TestEnum::Bit0);

        f ^= TestEnum::Bit1;
        CHECK(f == TestEnum::None);

        f = ~TestEnum::None;
        CHECK(f.IsSet(TestEnum::Bit0));
        CHECK(f.IsSet(TestEnum::Bit1));
        CHECK(f.IsSet(TestEnum::Bit2));
        CHECK(f.IsSet(TestEnum::Bit3));
    }

    SECTION("Boolean Conversion")
    {
        Flags<TestEnum> f;
        CHECK(!f);
        
        f = TestEnum::Bit3;
        CHECK(f);
    }
}

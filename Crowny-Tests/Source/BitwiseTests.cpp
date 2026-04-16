#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "cwpch.h"
#include "Crowny/Utils/Bitwise.h"

using namespace Crowny;

TEST_CASE("Bitwise Utilities", "[Utils][Bitwise]")
{
    SECTION("IsPow2")
    {
        CHECK(Bitwise::IsPow2(1));
        CHECK(Bitwise::IsPow2(2));
        CHECK(Bitwise::IsPow2(4));
        CHECK(Bitwise::IsPow2(1024));
        
        CHECK_FALSE(Bitwise::IsPow2(0)); // 0 is not a power of 2
        CHECK_FALSE(Bitwise::IsPow2(3));
        CHECK_FALSE(Bitwise::IsPow2(7));
        CHECK_FALSE(Bitwise::IsPow2(1023));
        CHECK_FALSE(Bitwise::IsPow2(1025));
    }

    SECTION("Unorm/Uint Conversions")
    {
        // 8-bit unorm
        CHECK(Bitwise::UnormToUint(0.0f, 8) == 0);
        CHECK(Bitwise::UnormToUint(1.0f, 8) == 255);
        CHECK(Bitwise::UnormToUint(0.5f, 8) == 128);
        
        CHECK_THAT(Bitwise::UintToUnorm(0, 8), Catch::Matchers::WithinAbs(0.0f, 0.0001f));
        CHECK_THAT(Bitwise::UintToUnorm(255, 8), Catch::Matchers::WithinAbs(1.0f, 0.0001f));
        CHECK_THAT(Bitwise::UintToUnorm(128, 8), Catch::Matchers::WithinAbs(128.0f/255.0f, 0.0001f));
    }

    SECTION("Snorm/Uint Conversions")
    {
        // 8-bit snorm
        CHECK(Bitwise::SnormToUint(-1.0f, 8) == 0);
        CHECK(Bitwise::SnormToUint(1.0f, 8) == 255);
        CHECK(Bitwise::SnormToUint(0.0f, 8) == 128);

        CHECK_THAT(Bitwise::UintToSnorm(0, 8), Catch::Matchers::WithinAbs(-1.0f, 0.0001f));
        CHECK_THAT(Bitwise::UintToSnorm(255, 8), Catch::Matchers::WithinAbs(1.0f, 0.0001f));
        CHECK_THAT(Bitwise::UintToSnorm(128, 8), Catch::Matchers::WithinAbs(0.003921f, 0.005f)); // Approx 0
    }

    SECTION("Int Read/Write")
    {
        uint32_t buffer = 0;
        
        Bitwise::IntWrite(&buffer, 1, 0xAA);
        CHECK((buffer & 0xFF) == 0xAA);
        CHECK(Bitwise::IntRead(&buffer, 1) == 0xAA);

        buffer = 0;
        Bitwise::IntWrite(&buffer, 2, 0xABCD);
        CHECK((buffer & 0xFFFF) == 0xABCD);
        CHECK(Bitwise::IntRead(&buffer, 2) == 0xABCD);

        buffer = 0;
        Bitwise::IntWrite(&buffer, 4, 0xDEADBEEF);
        CHECK(buffer == 0xDEADBEEF);
        CHECK(Bitwise::IntRead(&buffer, 4) == 0xDEADBEEF);
    }

    SECTION("Half Float Conversions")
    {
        float original = 1.0f;
        uint16_t half = Bitwise::FloatToHalf(original);
        CHECK_THAT(Bitwise::HalfToFloat(half), Catch::Matchers::WithinAbs(1.0f, 0.001f));

        original = 0.5f;
        half = Bitwise::FloatToHalf(original);
        CHECK_THAT(Bitwise::HalfToFloat(half), Catch::Matchers::WithinAbs(0.5f, 0.001f));

        original = -2.0f;
        half = Bitwise::FloatToHalf(original);
        CHECK_THAT(Bitwise::HalfToFloat(half), Catch::Matchers::WithinAbs(-2.0f, 0.001f));

        original = 0.0f;
        half = Bitwise::FloatToHalf(original);
        CHECK(Bitwise::HalfToFloat(half) == 0.0f);
    }
}

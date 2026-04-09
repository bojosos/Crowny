#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Crowny/Common/Color.h"

using namespace Crowny;

TEST_CASE("Color::Basic", "[Color]")
{
    SECTION("FromRGBA")
    {
        glm::vec4 rgba(1.0f, 0.5f, 0.0f, 1.0f);
        Color c = Color::FromRGBA(rgba);
        
        uint32_t val = (uint32_t)c;
        // Expected ARGB: A=255, R=255, G=127, B=0 = 0xFFFF7F00
        CHECK(val == 0xFFFF7F00);
    }

    SECTION("FromRGB")
    {
        glm::vec3 rgb(1.0f, 0.0f, 1.0f);
        Color c = Color::FromRGB(rgb);
        
        uint32_t val = (uint32_t)c;
        // Expected ARGB: A=255, R=255, G=0, B=255 = 0xFFFF00FF
        CHECK(val == 0xFFFF00FF);
    }

    SECTION("FromHex")
    {
        Color c = Color::FromHex(0x11223344);
        CHECK((uint32_t)c == 0x11223344);
    }
}

TEST_CASE("Color::Conversion", "[Color]")
{
    Color c = Color::FromHex(0x80FF4000); // A=128, R=255, G=64, B=0
    
    glm::vec4 rgba = (glm::vec4)c;
    
    CHECK_THAT(rgba.a, Catch::Matchers::WithinRel(128.0f / 255.0f, 0.001f));
    CHECK_THAT(rgba.r, Catch::Matchers::WithinRel(1.0f, 0.001f));
    CHECK_THAT(rgba.g, Catch::Matchers::WithinRel(64.0f / 255.0f, 0.001f));
    CHECK_THAT(rgba.b, Catch::Matchers::WithinRel(0.0f, 0.001f));
}

TEST_CASE("Color::StaticColors", "[Color]")
{
    CHECK((uint32_t)Color::Black == 0xFF000000);
    CHECK((uint32_t)Color::White == 0xFFFFFFFF);
    CHECK((uint32_t)Color::Red == 0xFFFF0000);
    // ... potentially others
}

TEST_CASE("Color::RoundtripIdentity", "[Color]")
{
    SECTION("Full saturation roundtrip")
    {
        glm::vec4 original(1.0f, 0.0f, 0.5f, 1.0f);
        Color c = Color::FromRGBA(original);
        glm::vec4 result = (glm::vec4)c;

        // int(0.5 * 255) = 127, 127/255 = 0.498..., so we allow ~1/255 tolerance
        CHECK_THAT(result.r, Catch::Matchers::WithinAbs(original.r, 1.0f / 255.0f + 0.001f));
        CHECK_THAT(result.g, Catch::Matchers::WithinAbs(original.g, 1.0f / 255.0f + 0.001f));
        CHECK_THAT(result.b, Catch::Matchers::WithinAbs(original.b, 1.0f / 255.0f + 0.001f));
        CHECK_THAT(result.a, Catch::Matchers::WithinAbs(original.a, 1.0f / 255.0f + 0.001f));
    }

    SECTION("Black roundtrip")
    {
        glm::vec4 original(0.0f, 0.0f, 0.0f, 1.0f);
        Color c = Color::FromRGBA(original);
        glm::vec4 result = (glm::vec4)c;

        CHECK_THAT(result.r, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        CHECK_THAT(result.g, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        CHECK_THAT(result.b, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        CHECK_THAT(result.a, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    }

    SECTION("White roundtrip")
    {
        glm::vec4 original(1.0f, 1.0f, 1.0f, 1.0f);
        Color c = Color::FromRGBA(original);
        glm::vec4 result = (glm::vec4)c;

        CHECK_THAT(result.r, Catch::Matchers::WithinAbs(1.0f, 0.001f));
        CHECK_THAT(result.g, Catch::Matchers::WithinAbs(1.0f, 0.001f));
        CHECK_THAT(result.b, Catch::Matchers::WithinAbs(1.0f, 0.001f));
        CHECK_THAT(result.a, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    }

    SECTION("Transparent roundtrip")
    {
        glm::vec4 original(0.0f, 0.0f, 0.0f, 0.0f);
        Color c = Color::FromRGBA(original);
        glm::vec4 result = (glm::vec4)c;

        CHECK_THAT(result.r, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        CHECK_THAT(result.g, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        CHECK_THAT(result.b, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        CHECK_THAT(result.a, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }
}

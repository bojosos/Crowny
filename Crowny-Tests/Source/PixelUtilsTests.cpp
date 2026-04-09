#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Crowny/Utils/PixelUtils.h"

using namespace Crowny;

TEST_CASE("PixelData::Basic", "[PixelUtils]")
{
    uint32_t width = 16, height = 16, depth = 1;
    TextureFormat format = TextureFormat::RGBA8;
    
    PixelData pd(width, height, depth, format);
    pd.AllocateInternalBuffer();

    CHECK(pd.GetWidth() == width);
    CHECK(pd.GetHeight() == height);
    CHECK(pd.GetDepth() == depth);
    CHECK(pd.GetFormat() == format);
    CHECK(pd.GetData() != nullptr);
    CHECK(pd.GetSize() == width * height * depth * 4);
}

TEST_CASE("PixelUtils::PackUnpack", "[PixelUtils]")
{
    SECTION("RGBA8")
    {
        uint8_t pixel[4];
        float r = 1.0f, g = 0.5f, b = 0.0f, a = 1.0f;
        PixelUtils::PackPixel(r, g, b, a, TextureFormat::RGBA8, pixel);
        
        CHECK(pixel[0] == 255);
        CHECK(pixel[1] == 128); // 0.5 * 255 = 127.5, rounds to 128
        CHECK(pixel[2] == 0);
        CHECK(pixel[3] == 255);

        float ur, ug, ub, ua;
        PixelUtils::UnpackPixel(&ur, &ug, &ub, &ua, TextureFormat::RGBA8, pixel);
        
        CHECK_THAT(ur, Catch::Matchers::WithinRel(1.0f, 0.01f));
        CHECK_THAT(ug, Catch::Matchers::WithinRel(0.5f, 0.01f));
        CHECK_THAT(ub, Catch::Matchers::WithinRel(0.0f, 0.01f));
        CHECK_THAT(ua, Catch::Matchers::WithinRel(1.0f, 0.01f));
    }

    SECTION("R8")
    {
        uint8_t pixel[1];
        PixelUtils::PackPixel(0.25f, 0.0f, 0.0f, 1.0f, TextureFormat::R8, pixel);
        CHECK(pixel[0] == 64); // 0.25 * 255 = 63.75, rounds to 64

        float r, g, b, a;
        PixelUtils::UnpackPixel(&r, &g, &b, &a, TextureFormat::R8, pixel);
        CHECK_THAT(r, Catch::Matchers::WithinRel(0.25f, 0.01f));
        CHECK(g == 0.0f);
        CHECK(b == 0.0f);
        CHECK(a == 1.0f);
    }
}

TEST_CASE("PixelData::ColorAt", "[PixelUtils]")
{
    PixelData pd(2, 2, 1, TextureFormat::RGBA8);
    pd.AllocateInternalBuffer();

    glm::vec4 color(1.0f, 0.0f, 0.0f, 1.0f);
    pd.SetColorAt(1, 1, color);

    glm::vec4 readColor = pd.GetColorAt(1, 1);
    CHECK_THAT(readColor.r, Catch::Matchers::WithinRel(1.0f, 0.01f));
    CHECK_THAT(readColor.g, Catch::Matchers::WithinRel(0.0f, 0.01f));
    CHECK_THAT(readColor.b, Catch::Matchers::WithinRel(0.0f, 0.01f));
    CHECK_THAT(readColor.a, Catch::Matchers::WithinRel(1.0f, 0.01f));

    glm::vec4 otherColor = pd.GetColorAt(0, 0);
    CHECK(otherColor.r == 0.0f);
    CHECK(otherColor.a == 0.0f); // RGBA8 defaults to 0 if not set
}

TEST_CASE("PixelUtils::Conversion", "[PixelUtils]")
{
    PixelData src(1, 1, 1, TextureFormat::RGBA8);
    src.AllocateInternalBuffer();
    src.SetColorAt(0, 0, glm::vec4(1.0f, 0.5f, 0.25f, 1.0f));

    PixelData dst(1, 1, 1, TextureFormat::BGRA8);
    dst.AllocateInternalBuffer();

    PixelUtils::ConvertPixels(src, dst);

    glm::vec4 convertedColor = dst.GetColorAt(0, 0);
    CHECK_THAT(convertedColor.r, Catch::Matchers::WithinRel(1.0f, 0.01f));
    CHECK_THAT(convertedColor.g, Catch::Matchers::WithinRel(0.5f, 0.01f));
    CHECK_THAT(convertedColor.b, Catch::Matchers::WithinRel(0.25f, 0.01f));
    CHECK_THAT(convertedColor.a, Catch::Matchers::WithinRel(1.0f, 0.01f));
    
    // Check BGRA8 byte order: B, G, R, A
    uint8_t* raw = dst.GetData();
    CHECK(raw[0] == 64);  // B (0.25 * 255 = 63.75 -> 64)
    CHECK(raw[1] == 128); // G (0.5 * 255 = 127.5 -> 128)
    CHECK(raw[2] == 255); // R (1.0 * 255 = 255)
    CHECK(raw[3] == 255); // A (1.0 * 255 = 255)
}

TEST_CASE("PixelUtils::IsCompressedFormat", "[PixelUtils]")
{
    SECTION("BC formats are compressed")
    {
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::BC1));
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::BC1a));
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::BC2));
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::BC3));
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::BC4));
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::BC5));
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::BC6H));
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::BC7));
    }

    SECTION("Uncompressed formats are not compressed")
    {
        CHECK_FALSE(PixelUtils::IsCompressedFormat(TextureFormat::RGBA8));
        CHECK_FALSE(PixelUtils::IsCompressedFormat(TextureFormat::R8));
        CHECK_FALSE(PixelUtils::IsCompressedFormat(TextureFormat::RG8));
        CHECK_FALSE(PixelUtils::IsCompressedFormat(TextureFormat::RGB8));
        CHECK_FALSE(PixelUtils::IsCompressedFormat(TextureFormat::BGRA8));
        CHECK_FALSE(PixelUtils::IsCompressedFormat(TextureFormat::RGBA16F));
        CHECK_FALSE(PixelUtils::IsCompressedFormat(TextureFormat::RGBA32F));
        CHECK_FALSE(PixelUtils::IsCompressedFormat(TextureFormat::DEPTH32F));
    }
}

TEST_CASE("PixelUtils::BlockDimensions", "[PixelUtils]")
{
    SECTION("Compressed formats have 4x4 block dimensions")
    {
        CHECK(PixelUtils::GetBlockDimensions(TextureFormat::BC1) == glm::ivec2(4, 4));
        CHECK(PixelUtils::GetBlockDimensions(TextureFormat::BC1a) == glm::ivec2(4, 4));
        CHECK(PixelUtils::GetBlockDimensions(TextureFormat::BC2) == glm::ivec2(4, 4));
        CHECK(PixelUtils::GetBlockDimensions(TextureFormat::BC3) == glm::ivec2(4, 4));
        CHECK(PixelUtils::GetBlockDimensions(TextureFormat::BC4) == glm::ivec2(4, 4));
        CHECK(PixelUtils::GetBlockDimensions(TextureFormat::BC5) == glm::ivec2(4, 4));
        CHECK(PixelUtils::GetBlockDimensions(TextureFormat::BC6H) == glm::ivec2(4, 4));
        CHECK(PixelUtils::GetBlockDimensions(TextureFormat::BC7) == glm::ivec2(4, 4));
    }

    SECTION("Uncompressed formats have 1x1 block dimensions")
    {
        CHECK(PixelUtils::GetBlockDimensions(TextureFormat::RGBA8) == glm::ivec2(1, 1));
        CHECK(PixelUtils::GetBlockDimensions(TextureFormat::R8) == glm::ivec2(1, 1));
        CHECK(PixelUtils::GetBlockDimensions(TextureFormat::BGRA8) == glm::ivec2(1, 1));
    }
}

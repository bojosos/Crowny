#include "Crowny/Utils/PixelUtils.h"
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <string_view>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

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

    REQUIRE(PixelUtils::ConvertPixels(src, dst));

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

TEST_CASE("PixelUtils::PackUnpack::RG8", "[PixelUtils]")
{
    uint8_t pixel[2];
    PixelUtils::PackPixel(0.75f, 0.25f, 0.0f, 1.0f, TextureFormat::RG8, pixel);
    CHECK(pixel[0] == 191); // 0.75 * 255 = 191.25 -> 191
    CHECK(pixel[1] == 64);  // 0.25 * 255 = 63.75 -> 64

    float r, g, b, a;
    PixelUtils::UnpackPixel(&r, &g, &b, &a, TextureFormat::RG8, pixel);
    CHECK_THAT(r, Catch::Matchers::WithinRel(0.75f, 0.01f));
    CHECK_THAT(g, Catch::Matchers::WithinRel(0.25f, 0.01f));
    CHECK(b == 0.0f);
    CHECK(a == 1.0f);
}

TEST_CASE("PixelUtils::PackUnpack::RGB8", "[PixelUtils]")
{
    uint8_t pixel[3];
    PixelUtils::PackPixel(1.0f, 0.5f, 0.25f, 1.0f, TextureFormat::RGB8, pixel);
    CHECK(pixel[0] == 255);
    CHECK(pixel[1] == 128);
    CHECK(pixel[2] == 64);

    float r, g, b, a;
    PixelUtils::UnpackPixel(&r, &g, &b, &a, TextureFormat::RGB8, pixel);
    CHECK_THAT(r, Catch::Matchers::WithinRel(1.0f, 0.01f));
    CHECK_THAT(g, Catch::Matchers::WithinRel(0.5f, 0.01f));
    CHECK_THAT(b, Catch::Matchers::WithinRel(0.25f, 0.01f));
    CHECK(a == 1.0f);
}

TEST_CASE("PixelUtils::PackUnpack::BGRA8", "[PixelUtils]")
{
    uint8_t pixel[4];
    PixelUtils::PackPixel(1.0f, 0.5f, 0.25f, 1.0f, TextureFormat::BGRA8, pixel);
    // BGRA layout: B, G, R, A
    CHECK(pixel[0] == 64);  // B = 0.25
    CHECK(pixel[1] == 128); // G = 0.5
    CHECK(pixel[2] == 255); // R = 1.0
    CHECK(pixel[3] == 255); // A = 1.0

    float r, g, b, a;
    PixelUtils::UnpackPixel(&r, &g, &b, &a, TextureFormat::BGRA8, pixel);
    CHECK_THAT(r, Catch::Matchers::WithinRel(1.0f, 0.01f));
    CHECK_THAT(g, Catch::Matchers::WithinRel(0.5f, 0.01f));
    CHECK_THAT(b, Catch::Matchers::WithinRel(0.25f, 0.01f));
    CHECK_THAT(a, Catch::Matchers::WithinRel(1.0f, 0.01f));
}

TEST_CASE("PixelUtils::PackUnpack::RGBA16F", "[PixelUtils]")
{
    uint8_t pixel[8];
    PixelUtils::PackPixel(1.0f, 0.5f, 0.25f, 0.0f, TextureFormat::RGBA16F, pixel);

    float r, g, b, a;
    PixelUtils::UnpackPixel(&r, &g, &b, &a, TextureFormat::RGBA16F, pixel);
    CHECK_THAT(r, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    CHECK_THAT(g, Catch::Matchers::WithinAbs(0.5f, 0.001f));
    CHECK_THAT(b, Catch::Matchers::WithinAbs(0.25f, 0.001f));
    CHECK_THAT(a, Catch::Matchers::WithinAbs(0.0f, 0.001f));
}

TEST_CASE("PixelUtils::PackUnpack::RGBA32F", "[PixelUtils]")
{
    uint8_t pixel[16];
    float wr = 3.14f, wg = -1.0f, wb = 0.0f, wa = 1000.0f;
    PixelUtils::PackPixel(wr, wg, wb, wa, TextureFormat::RGBA32F, pixel);

    float r, g, b, a;
    PixelUtils::UnpackPixel(&r, &g, &b, &a, TextureFormat::RGBA32F, pixel);
    CHECK(r == wr);
    CHECK(g == wg);
    CHECK(b == wb);
    CHECK(a == wa);
}

TEST_CASE("PixelUtils::PackUnpack::RGB32F", "[PixelUtils]")
{
    uint8_t pixel[12];
    float wr = 1.5f, wg = -2.0f, wb = 0.001f;
    PixelUtils::PackPixel(wr, wg, wb, 1.0f, TextureFormat::RGB32F, pixel);

    float r, g, b, a;
    PixelUtils::UnpackPixel(&r, &g, &b, &a, TextureFormat::RGB32F, pixel);
    CHECK(r == wr);
    CHECK(g == wg);
    CHECK(b == wb);
    CHECK(a == 1.0f); // alpha filled to 1 by UnpackPixel for 3-component formats
}

TEST_CASE("PixelUtils::PackUnpack::RG16F", "[PixelUtils]")
{
    uint8_t pixel[4];
    PixelUtils::PackPixel(1.0f, 0.5f, 0.0f, 1.0f, TextureFormat::RG16F, pixel);

    float r, g, b, a;
    PixelUtils::UnpackPixel(&r, &g, &b, &a, TextureFormat::RG16F, pixel);
    CHECK_THAT(r, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    CHECK_THAT(g, Catch::Matchers::WithinAbs(0.5f, 0.001f));
    CHECK(b == 0.0f);
    CHECK(a == 1.0f);
}

TEST_CASE("PixelUtils::PackUnpack::RGBA16", "[PixelUtils]")
{
    uint8_t pixel[8];
    PixelUtils::PackPixel(1.0f, 0.5f, 0.25f, 0.125f, TextureFormat::RGBA16, pixel);

    float r, g, b, a;
    PixelUtils::UnpackPixel(&r, &g, &b, &a, TextureFormat::RGBA16, pixel);
    CHECK_THAT(r, Catch::Matchers::WithinAbs(1.0f, 1.0f / 65535.0f));
    CHECK_THAT(g, Catch::Matchers::WithinAbs(0.5f, 1.0f / 65535.0f));
    CHECK_THAT(b, Catch::Matchers::WithinAbs(0.25f, 1.0f / 65535.0f));
    CHECK_THAT(a, Catch::Matchers::WithinAbs(0.125f, 1.0f / 65535.0f));
}

TEST_CASE("PixelUtils::GetNumBytes", "[PixelUtils]")
{
    CHECK(PixelUtils::GetNumBytes(TextureFormat::R8) == 1);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::RG8) == 2);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::RGB8) == 3);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::RGBA8) == 4);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::BGRA8) == 4);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::RG16F) == 4);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::RGBA16F) == 8);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::RGB32F) == 12);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::RGBA32F) == 16);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::RG32F) == 8);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::R32I) == 4);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::R32F) == 4);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::R16) == 2);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::RG16) == 4);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::RGB16) == 6);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::RGBA16) == 8);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::DEPTH32F) == 4);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::DEPTH24STENCIL8) == 4);
    // Compressed formats have no per-pixel byte count
    CHECK(PixelUtils::GetNumBytes(TextureFormat::BC1) == 0);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::BC3) == 0);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::BC7) == 0);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::ETC2_RGBA) == 0);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::ASTC4x4) == 0);
}

TEST_CASE("PixelUtils::GetBlockSize", "[PixelUtils]")
{
    // BC1/BC1a/BC4 = 8 bytes per 4x4 block
    CHECK(PixelUtils::GetBlockSize(TextureFormat::BC1) == 8);
    CHECK(PixelUtils::GetBlockSize(TextureFormat::BC1a) == 8);
    CHECK(PixelUtils::GetBlockSize(TextureFormat::BC4) == 8);
    // BC2/BC3/BC5/BC6H/BC7 = 16 bytes per 4x4 block
    CHECK(PixelUtils::GetBlockSize(TextureFormat::BC2) == 16);
    CHECK(PixelUtils::GetBlockSize(TextureFormat::BC3) == 16);
    CHECK(PixelUtils::GetBlockSize(TextureFormat::BC5) == 16);
    CHECK(PixelUtils::GetBlockSize(TextureFormat::BC6H) == 16);
    CHECK(PixelUtils::GetBlockSize(TextureFormat::BC7) == 16);
    CHECK(PixelUtils::GetBlockSize(TextureFormat::ETC2_RGB) == 8);
    CHECK(PixelUtils::GetBlockSize(TextureFormat::ETC2_R11) == 8);
    CHECK(PixelUtils::GetBlockSize(TextureFormat::ETC2_RGBA) == 16);
    CHECK(PixelUtils::GetBlockSize(TextureFormat::ETC2_RG11) == 16);
    CHECK(PixelUtils::GetBlockSize(TextureFormat::ASTC4x4) == 16);
    // Uncompressed formats return per-pixel byte count
    CHECK(PixelUtils::GetBlockSize(TextureFormat::RGBA8) == 4);
    CHECK(PixelUtils::GetBlockSize(TextureFormat::R8) == 1);
}

TEST_CASE("PixelUtils::GetMemSize", "[PixelUtils]")
{
    CHECK(PixelUtils::GetMemSize(4, 4, 1, TextureFormat::RGBA8) == 64);
    CHECK(PixelUtils::GetMemSize(1, 1, 1, TextureFormat::RGBA32F) == 16);
    CHECK(PixelUtils::GetMemSize(8, 8, 1, TextureFormat::R8) == 64);
    // BC1: 4x4 block = 8 bytes; a 4x4 image is one block
    CHECK(PixelUtils::GetMemSize(4, 4, 1, TextureFormat::BC1) == 8);
    // BC1: 8x8 image = 4 blocks = 32 bytes
    CHECK(PixelUtils::GetMemSize(8, 8, 1, TextureFormat::BC1) == 32);
    // BC3: 4x4 block = 16 bytes
    CHECK(PixelUtils::GetMemSize(4, 4, 1, TextureFormat::BC3) == 16);
    // Smaller than a full block still rounds up to one block
    CHECK(PixelUtils::GetMemSize(2, 2, 1, TextureFormat::BC1) == 8);
}

TEST_CASE("PixelUtils::GetBitDepths", "[PixelUtils]")
{
    int rgba[4];

    PixelUtils::GetBitDepths(TextureFormat::RGBA8, rgba);
    CHECK(rgba[0] == 8);
    CHECK(rgba[1] == 8);
    CHECK(rgba[2] == 8);
    CHECK(rgba[3] == 8);

    PixelUtils::GetBitDepths(TextureFormat::RGB8, rgba);
    CHECK(rgba[0] == 8);
    CHECK(rgba[1] == 8);
    CHECK(rgba[2] == 8);
    CHECK(rgba[3] == 0);

    PixelUtils::GetBitDepths(TextureFormat::R8, rgba);
    CHECK(rgba[0] == 8);
    CHECK(rgba[1] == 0);
    CHECK(rgba[2] == 0);
    CHECK(rgba[3] == 0);

    PixelUtils::GetBitDepths(TextureFormat::RGBA32F, rgba);
    CHECK(rgba[0] == 32);
    CHECK(rgba[1] == 32);
    CHECK(rgba[2] == 32);
    CHECK(rgba[3] == 32);

    PixelUtils::GetBitDepths(TextureFormat::RGB32F, rgba);
    CHECK(rgba[0] == 32);
    CHECK(rgba[1] == 32);
    CHECK(rgba[2] == 32);
    CHECK(rgba[3] == 0);

    PixelUtils::GetBitDepths(TextureFormat::RGBA16F, rgba);
    CHECK(rgba[0] == 16);
    CHECK(rgba[1] == 16);
    CHECK(rgba[2] == 16);
    CHECK(rgba[3] == 16);

    PixelUtils::GetBitDepths(TextureFormat::DEPTH24STENCIL8, rgba);
    CHECK(rgba[0] == 24);
    CHECK(rgba[1] == 8);
    CHECK(rgba[2] == 0);
    CHECK(rgba[3] == 0);
}

TEST_CASE("PixelUtils::GetMipSizeForLevel", "[PixelUtils]")
{
    uint32_t w, h, d;

    PixelUtils::GetMipSizeForLevel(256, 128, 1, 0, w, h, d);
    CHECK(w == 256);
    CHECK(h == 128);
    CHECK(d == 1);

    PixelUtils::GetMipSizeForLevel(256, 128, 1, 1, w, h, d);
    CHECK(w == 128);
    CHECK(h == 64);
    CHECK(d == 1);

    PixelUtils::GetMipSizeForLevel(256, 128, 1, 2, w, h, d);
    CHECK(w == 64);
    CHECK(h == 32);
    CHECK(d == 1);

    // Dimension clamps at 1
    PixelUtils::GetMipSizeForLevel(4, 1, 1, 3, w, h, d);
    CHECK(w == 1);
    CHECK(h == 1);
    CHECK(d == 1);
}

TEST_CASE("PixelUtils::D24S8 stencil mask", "[PixelUtils]")
{
    // Verify depth and stencil are packed/unpacked independently without overlap
    uint8_t pixel[4] = {};

    // Full depth (all 24 bits), zero stencil
    PixelUtils::PackPixel(1.0f, 0.0f, 0.0f, 1.0f, TextureFormat::DEPTH24STENCIL8, pixel);
    float depth, stencil, b, a;
    PixelUtils::UnpackPixel(&depth, &stencil, &b, &a, TextureFormat::DEPTH24STENCIL8, pixel);
    CHECK_THAT(depth, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    CHECK_THAT(stencil, Catch::Matchers::WithinAbs(0.0f, 0.001f));

    // Zero depth, full stencil (all 8 bits)
    std::memset(pixel, 0, 4);
    PixelUtils::PackPixel(0.0f, 1.0f, 0.0f, 1.0f, TextureFormat::DEPTH24STENCIL8, pixel);
    PixelUtils::UnpackPixel(&depth, &stencil, &b, &a, TextureFormat::DEPTH24STENCIL8, pixel);
    CHECK_THAT(depth, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    CHECK_THAT(stencil, Catch::Matchers::WithinAbs(1.0f, 0.001f));
}

TEST_CASE("PixelData::RGB8 ColorAt", "[PixelUtils]")
{
    PixelData pd(4, 4, 1, TextureFormat::RGB8);
    pd.AllocateInternalBuffer();

    glm::vec4 color(0.5f, 0.25f, 1.0f, 1.0f);
    pd.SetColorAt(2, 3, color);

    glm::vec4 read = pd.GetColorAt(2, 3);
    CHECK_THAT(read.r, Catch::Matchers::WithinRel(0.5f, 0.01f));
    CHECK_THAT(read.g, Catch::Matchers::WithinRel(0.25f, 0.01f));
    CHECK_THAT(read.b, Catch::Matchers::WithinRel(1.0f, 0.01f));

    // Adjacent pixel must remain zero
    glm::vec4 other = pd.GetColorAt(1, 3);
    CHECK(other.r == 0.0f);
    CHECK(other.g == 0.0f);
    CHECK(other.b == 0.0f);
}

TEST_CASE("PixelData::RGBA32F ColorAt", "[PixelUtils]")
{
    PixelData pd(2, 2, 1, TextureFormat::RGBA32F);
    pd.AllocateInternalBuffer();

    glm::vec4 color(3.14f, -1.0f, 1000.0f, 0.5f);
    pd.SetColorAt(0, 1, color);

    glm::vec4 read = pd.GetColorAt(0, 1);
    CHECK(read.r == color.r);
    CHECK(read.g == color.g);
    CHECK(read.b == color.b);
    CHECK(read.a == color.a);
}

TEST_CASE("PixelData::GetSize", "[PixelUtils]")
{
    CHECK(PixelData(8, 8, 1, TextureFormat::RGBA8).GetSize() == 8 * 8 * 4);
    CHECK(PixelData(4, 4, 1, TextureFormat::RGB8).GetSize() == 4 * 4 * 3);
    CHECK(PixelData(4, 4, 1, TextureFormat::R8).GetSize() == 4 * 4 * 1);
    CHECK(PixelData(2, 2, 2, TextureFormat::RGBA32F).GetSize() == 2 * 2 * 2 * 16);
}

TEST_CASE("PixelData::IsNice", "[PixelUtils]")
{
    PixelData pd(4, 4, 1, TextureFormat::RGBA8);
    pd.AllocateInternalBuffer();
    CHECK(pd.IsNice());

    // Custom pitch larger than optimal makes it non-nice
    PixelData pd2(4, 4, 1, TextureFormat::RGBA8);
    pd2.SetRowPitch(4 * 4 + 16); // 16-byte row padding
    pd2.SetSlicePitch((4 * 4 + 16) * 4);
    CHECK_FALSE(pd2.IsNice());
}

TEST_CASE("PixelUtils::Conversion::RGB8toRGBA8", "[PixelUtils]")
{
    PixelData src(1, 1, 1, TextureFormat::RGB8);
    src.AllocateInternalBuffer();
    src.SetColorAt(0, 0, glm::vec4(1.0f, 0.5f, 0.25f, 1.0f));

    PixelData dst(1, 1, 1, TextureFormat::RGBA8);
    dst.AllocateInternalBuffer();
    REQUIRE(PixelUtils::ConvertPixels(src, dst));

    glm::vec4 c = dst.GetColorAt(0, 0);
    CHECK_THAT(c.r, Catch::Matchers::WithinRel(1.0f, 0.01f));
    CHECK_THAT(c.g, Catch::Matchers::WithinRel(0.5f, 0.01f));
    CHECK_THAT(c.b, Catch::Matchers::WithinRel(0.25f, 0.01f));
    CHECK_THAT(c.a, Catch::Matchers::WithinRel(1.0f, 0.01f));
}

TEST_CASE("PixelUtils::Conversion::RGBA8toRGBA32F", "[PixelUtils]")
{
    PixelData src(1, 1, 1, TextureFormat::RGBA8);
    src.AllocateInternalBuffer();
    src.SetColorAt(0, 0, glm::vec4(1.0f, 0.0f, 0.5f, 1.0f));

    PixelData dst(1, 1, 1, TextureFormat::RGBA32F);
    dst.AllocateInternalBuffer();
    REQUIRE(PixelUtils::ConvertPixels(src, dst));

    glm::vec4 c = dst.GetColorAt(0, 0);
    CHECK_THAT(c.r, Catch::Matchers::WithinRel(1.0f, 0.01f));
    CHECK_THAT(c.g, Catch::Matchers::WithinAbs(0.0f, 0.01f));
    CHECK_THAT(c.b, Catch::Matchers::WithinRel(0.5f, 0.01f));
    CHECK_THAT(c.a, Catch::Matchers::WithinRel(1.0f, 0.01f));
}

TEST_CASE("PixelUtils::Conversion::SameFormat::MultiplePixels", "[PixelUtils]")
{
    // Same-format copy across a 3x1 image verifies all pixels are copied, not just the first
    PixelData src(3, 1, 1, TextureFormat::RGBA8);
    src.AllocateInternalBuffer();
    src.SetColorAt(0, 0, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    src.SetColorAt(1, 0, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    src.SetColorAt(2, 0, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

    PixelData dst(3, 1, 1, TextureFormat::RGBA8);
    dst.AllocateInternalBuffer();
    REQUIRE(PixelUtils::ConvertPixels(src, dst));

    glm::vec4 c0 = dst.GetColorAt(0, 0);
    glm::vec4 c1 = dst.GetColorAt(1, 0);
    glm::vec4 c2 = dst.GetColorAt(2, 0);
    CHECK_THAT(c0.r, Catch::Matchers::WithinRel(1.0f, 0.01f));
    CHECK_THAT(c1.g, Catch::Matchers::WithinRel(1.0f, 0.01f));
    CHECK_THAT(c2.b, Catch::Matchers::WithinRel(1.0f, 0.01f));
}

TEST_CASE("PixelUtils::Conversion::SizeMismatch", "[PixelUtils]")
{
    PixelData src(2, 2, 1, TextureFormat::RGBA8);
    src.AllocateInternalBuffer();
    PixelData dst(4, 4, 1, TextureFormat::RGBA8);
    dst.AllocateInternalBuffer();

    CHECK_FALSE(PixelUtils::ConvertPixels(src, dst));
}

TEST_CASE("PixelUtils::IsCompressedFormat", "[PixelUtils]")
{
    SECTION("GPU block formats are compressed")
    {
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::BC1));
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::BC1a));
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::BC2));
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::BC3));
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::BC4));
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::BC5));
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::BC6H));
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::BC7));
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::ETC2_RGB));
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::ETC2_RGBA));
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::ETC2_R11));
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::ETC2_RG11));
        CHECK(PixelUtils::IsCompressedFormat(TextureFormat::ASTC4x4));
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
        CHECK(PixelUtils::GetBlockDimensions(TextureFormat::ETC2_RGBA) == glm::ivec2(4, 4));
        CHECK(PixelUtils::GetBlockDimensions(TextureFormat::ASTC4x4) == glm::ivec2(4, 4));
    }

    SECTION("Uncompressed formats have 1x1 block dimensions")
    {
        CHECK(PixelUtils::GetBlockDimensions(TextureFormat::RGBA8) == glm::ivec2(1, 1));
        CHECK(PixelUtils::GetBlockDimensions(TextureFormat::R8) == glm::ivec2(1, 1));
        CHECK(PixelUtils::GetBlockDimensions(TextureFormat::BGRA8) == glm::ivec2(1, 1));
    }
}

TEST_CASE("PixelUtils::FormatMetadata", "[PixelUtils]")
{
    CHECK(PixelUtils::IsValidFormat(TextureFormat::RGBA8));
    CHECK_FALSE(PixelUtils::IsValidFormat(TextureFormat::NONE));
    CHECK(std::string_view(PixelUtils::GetFormatName(TextureFormat::BC6H)) == "BC6H");
    CHECK(PixelUtils::GetComponentCount(TextureFormat::BC5) == 2);
    CHECK(PixelUtils::GetComponentCount(TextureFormat::BC1a) == 4);
    CHECK_FALSE(PixelUtils::HasAlpha(TextureFormat::BC1));
    CHECK(PixelUtils::HasAlpha(TextureFormat::BC1a));
    CHECK(PixelUtils::IsFloatFormat(TextureFormat::BC6H));
    CHECK((PixelUtils::GetFormatFlags(TextureFormat::R32I) & PFF_SIGNED) != 0);
}

TEST_CASE("PixelUtils::PitchAndMipChain", "[PixelUtils]")
{
    uint32_t rowPitch = 0;
    uint32_t slicePitch = 0;
    PixelUtils::GetPitch(5, 7, 1, TextureFormat::BC1, rowPitch, slicePitch);
    CHECK(rowPitch == 16);
    CHECK(slicePitch == 32);

    PixelUtils::GetPitch(0, 7, 1, TextureFormat::RGBA8, rowPitch, slicePitch);
    CHECK(rowPitch == 0);
    CHECK(slicePitch == 0);

    PixelUtils::GetPitch(std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max(), 1, TextureFormat::BC1, rowPitch, slicePitch);
    CHECK(rowPitch == 0);
    CHECK(slicePitch == 0);

    CHECK(PixelUtils::GetMaxMipCount(8, 4, 1) == 4);
    CHECK(PixelUtils::GetMipChainSize(4, 4, 1, TextureFormat::RGBA8) == 84);
    CHECK(PixelUtils::GetMipChainSize(8, 8, 1, TextureFormat::BC1) == 56);
    CHECK(PixelUtils::GetMipChainSize(4, 4, 1, TextureFormat::RGBA8, 0, 6) == 504);
    CHECK(PixelUtils::GetMipOffset(4, 4, 1, TextureFormat::RGBA8, 1) == 64);
    CHECK(PixelUtils::GetMipOffset(4, 4, 1, TextureFormat::RGBA8, 0, 1) == 84);
}

TEST_CASE("PixelUtils::GenerateMipChain", "[PixelUtils][Mips]")
{
    SECTION("Odd dimensions reach a one-pixel terminal level")
    {
        PixelData source(5, 3, 1, TextureFormat::RGBA8);
        source.AllocateInternalBuffer();
        for (uint32_t y = 0; y < source.GetHeight(); ++y)
            for (uint32_t x = 0; x < source.GetWidth(); ++x)
                source.SetColorAt(x, y, glm::vec4(static_cast<float>(x) / 4.0f, static_cast<float>(y) / 2.0f, 0.5f, 1.0f));

        Vector<Ref<PixelData>> mips;
        TextureMipGenerationOptions options;
        REQUIRE(PixelUtils::GenerateMipChain(source, options, mips));
        REQUIRE(mips.size() == 3);
        CHECK(mips[0]->GetWidth() == 5);
        CHECK(mips[0]->GetHeight() == 3);
        CHECK(mips[1]->GetWidth() == 2);
        CHECK(mips[1]->GetHeight() == 1);
        CHECK(mips[2]->GetWidth() == 1);
        CHECK(mips[2]->GetHeight() == 1);
    }

    SECTION("Color mips filter sRGB values in linear light")
    {
        PixelData source(2, 1, 1, TextureFormat::RGBA8);
        source.AllocateInternalBuffer();
        source.SetColorAt(0, 0, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        source.SetColorAt(1, 0, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

        TextureMipGenerationOptions options;
        options.Filter = TextureMipFilter::Box;
        options.SRGB = true;
        Vector<Ref<PixelData>> mips;
        REQUIRE(PixelUtils::GenerateMipChain(source, options, mips));
        REQUIRE(mips.size() == 2);
        const glm::vec4 average = mips[1]->GetColorAt(0, 0);
        CHECK_THAT(average.r, Catch::Matchers::WithinAbs(LinearToSRGB(0.5f), 0.015f));
        CHECK_THAT(average.g, Catch::Matchers::WithinAbs(LinearToSRGB(0.5f), 0.015f));
        CHECK_THAT(average.b, Catch::Matchers::WithinAbs(LinearToSRGB(0.5f), 0.015f));
    }

    SECTION("Transparent colors do not bleed into opaque texels")
    {
        PixelData source(2, 1, 1, TextureFormat::RGBA8);
        source.AllocateInternalBuffer();
        source.SetColorAt(0, 0, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
        source.SetColorAt(1, 0, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

        TextureMipGenerationOptions options;
        options.Filter = TextureMipFilter::Box;
        Vector<Ref<PixelData>> mips;
        REQUIRE(PixelUtils::GenerateMipChain(source, options, mips));
        const glm::vec4 average = mips[1]->GetColorAt(0, 0);
        CHECK(average.r < 0.02f);
        CHECK(average.b > 0.98f);
        CHECK_THAT(average.a, Catch::Matchers::WithinAbs(0.5f, 0.01f));
    }

    SECTION("Alpha coverage preserves the straight color of sparse opaque texels")
    {
        PixelData source(2, 2, 1, TextureFormat::RGBA8);
        source.AllocateInternalBuffer();
        source.SetColorAt(0, 0, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
        source.SetColorAt(1, 0, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
        source.SetColorAt(0, 1, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
        source.SetColorAt(1, 1, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));

        TextureMipGenerationOptions options;
        options.Filter = TextureMipFilter::Box;
        options.PreserveAlphaCoverage = true;
        options.AlphaCutoff = 0.5f;
        Vector<Ref<PixelData>> mips;
        REQUIRE(PixelUtils::GenerateMipChain(source, options, mips));
        REQUIRE(mips.size() == 2);

        const glm::vec4 average = mips[1]->GetColorAt(0, 0);
        CHECK(average.r < 0.02f);
        CHECK(average.b > 0.98f);
        CHECK_THAT(average.a, Catch::Matchers::WithinAbs(0.5f, 0.01f));
    }

    SECTION("Alpha coverage keeps fully transparent mips finite")
    {
        PixelData source(2, 2, 1, TextureFormat::RGBA32F);
        source.AllocateInternalBuffer();
        for (uint32_t y = 0; y < source.GetHeight(); ++y)
        {
            for (uint32_t x = 0; x < source.GetWidth(); ++x)
                source.SetColorAt(x, y, glm::vec4(1.0f, 0.0f, 1.0f, 0.0f));
        }

        TextureMipGenerationOptions options;
        options.Filter = TextureMipFilter::Box;
        options.PreserveAlphaCoverage = true;
        Vector<Ref<PixelData>> mips;
        REQUIRE(PixelUtils::GenerateMipChain(source, options, mips));
        REQUIRE(mips.size() == 2);

        const glm::vec4 average = mips[1]->GetColorAt(0, 0);
        CHECK(std::isfinite(average.r));
        CHECK(std::isfinite(average.g));
        CHECK(std::isfinite(average.b));
        CHECK(std::isfinite(average.a));
        CHECK(average == glm::vec4(0.0f));
    }

    SECTION("Normal-map mips are renormalized")
    {
        PixelData source(2, 2, 1, TextureFormat::RGBA8);
        source.AllocateInternalBuffer();
        source.SetColorAt(0, 0, glm::vec4(1.0f, 0.5f, 0.5f, 1.0f));
        source.SetColorAt(1, 0, glm::vec4(0.5f, 1.0f, 0.5f, 1.0f));
        source.SetColorAt(0, 1, glm::vec4(0.5f, 0.5f, 1.0f, 1.0f));
        source.SetColorAt(1, 1, glm::vec4(0.5f, 0.5f, 1.0f, 1.0f));

        TextureMipGenerationOptions options;
        options.Filter = TextureMipFilter::Box;
        options.Mode = TextureMipMode::NormalMap;
        Vector<Ref<PixelData>> mips;
        REQUIRE(PixelUtils::GenerateMipChain(source, options, mips));
        const glm::vec3 encoded(mips[1]->GetColorAt(0, 0));
        const glm::vec3 normal = encoded * 2.0f - 1.0f;
        CHECK_THAT(glm::length(normal), Catch::Matchers::WithinAbs(1.0f, 0.015f));
    }

    SECTION("Maximum level count is respected")
    {
        PixelData source(16, 8, 1, TextureFormat::R8);
        source.AllocateInternalBuffer();
        TextureMipGenerationOptions options;
        options.Mode = TextureMipMode::Data;
        options.MaxLevels = 2;
        Vector<Ref<PixelData>> mips;
        REQUIRE(PixelUtils::GenerateMipChain(source, options, mips));
        REQUIRE(mips.size() == 2);
        CHECK(mips.back()->GetWidth() == 8);
        CHECK(mips.back()->GetHeight() == 4);
    }

    SECTION("Invalid mode is rejected")
    {
        PixelData source(2, 2, 1, TextureFormat::RGBA8);
        source.AllocateInternalBuffer();
        TextureMipGenerationOptions options;
        options.Mode = TextureMipMode::Count;
        Vector<Ref<PixelData>> mips;
        String error;
        CHECK_FALSE(PixelUtils::GenerateMipChain(source, options, mips, &error));
        CHECK_FALSE(error.empty());
        CHECK(mips.empty());
    }
}

TEST_CASE("PixelUtils::Conversion::CompressedWithPadding", "[PixelUtils]")
{
    constexpr uint32_t rowPitch = 20;
    constexpr uint32_t slicePitch = 44;
    std::array<uint8_t, slicePitch> sourceBytes;
    std::array<uint8_t, slicePitch> destinationBytes;
    sourceBytes.fill(0xCC);
    destinationBytes.fill(0xEE);

    for (uint32_t i = 0; i < 16; i++)
    {
        sourceBytes[i] = static_cast<uint8_t>(i);
        sourceBytes[rowPitch + i] = static_cast<uint8_t>(0x40 + i);
    }

    Ref<PixelData> source = PixelData::CreateView(5, 7, 1, TextureFormat::BC1, sourceBytes.data(), rowPitch, slicePitch);
    Ref<PixelData> destination = PixelData::CreateView(5, 7, 1, TextureFormat::BC1, destinationBytes.data(), rowPitch, slicePitch);
    REQUIRE(source->HasValidPitches());
    REQUIRE(destination->HasValidPitches());
    REQUIRE(PixelUtils::ConvertPixels(*source, *destination));

    CHECK(std::memcmp(destinationBytes.data(), sourceBytes.data(), 16) == 0);
    CHECK(std::memcmp(destinationBytes.data() + rowPitch, sourceBytes.data() + rowPitch, 16) == 0);
    for (uint32_t i = 16; i < rowPitch; i++)
        CHECK(destinationBytes[i] == 0xEE);
    for (uint32_t i = rowPitch + 16; i < slicePitch; i++)
        CHECK(destinationBytes[i] == 0xEE);
}

TEST_CASE("PixelUtils::Conversion::Padded3D", "[PixelUtils]")
{
    constexpr uint32_t rowPitch = 12;
    constexpr uint32_t slicePitch = 28;
    std::array<uint8_t, slicePitch * 2> sourceBytes{};
    std::array<uint8_t, slicePitch * 2> destinationBytes;
    destinationBytes.fill(0xEE);

    Ref<PixelData> source = PixelData::CreateView(2, 2, 2, TextureFormat::RGBA8, sourceBytes.data(), rowPitch, slicePitch);
    Ref<PixelData> destination = PixelData::CreateView(2, 2, 2, TextureFormat::BGRA8, destinationBytes.data(), rowPitch, slicePitch);
    REQUIRE(source->TrySetColorAt(0, 0, 0, glm::vec4(1.0f, 0.25f, 0.5f, 1.0f)));
    REQUIRE(source->TrySetColorAt(1, 1, 1, glm::vec4(0.25f, 1.0f, 0.5f, 0.75f)));
    REQUIRE(PixelUtils::ConvertPixels(*source, *destination));

    glm::vec4 color;
    REQUIRE(destination->TryGetColorAt(0, 0, 0, color));
    CHECK_THAT(color.r, Catch::Matchers::WithinRel(1.0f, 0.01f));
    CHECK_THAT(color.g, Catch::Matchers::WithinRel(0.25f, 0.01f));
    CHECK_THAT(color.b, Catch::Matchers::WithinRel(0.5f, 0.01f));
    REQUIRE(destination->TryGetColorAt(1, 1, 1, color));
    CHECK_THAT(color.r, Catch::Matchers::WithinRel(0.25f, 0.01f));
    CHECK_THAT(color.g, Catch::Matchers::WithinRel(1.0f, 0.01f));
    CHECK_THAT(color.b, Catch::Matchers::WithinRel(0.5f, 0.01f));

    for (uint32_t z = 0; z < 2; z++)
    {
        const uint32_t base = z * slicePitch;
        for (uint32_t i = 8; i < rowPitch; i++)
            CHECK(destinationBytes[base + i] == 0xEE);
        for (uint32_t i = rowPitch + 8; i < slicePitch; i++)
            CHECK(destinationBytes[base + i] == 0xEE);
    }
}

TEST_CASE("PixelData::OwnershipAndValueSemantics", "[PixelUtils]")
{
    PixelData original(2, 1, 1, TextureFormat::RGBA8);
    original.AllocateInternalBuffer();
    REQUIRE(original.TrySetColorAt(0, 0, 0, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)));

    PixelData copy(original);
    REQUIRE(copy.OwnsBuffer());
    CHECK(copy.GetData() != original.GetData());
    REQUIRE(original.TrySetColorAt(0, 0, 0, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)));
    CHECK_THAT(copy.GetColorAt(0, 0).r, Catch::Matchers::WithinRel(1.0f, 0.01f));

    PixelData assigned;
    assigned = original;
    REQUIRE(assigned.OwnsBuffer());
    CHECK(assigned.GetData() != original.GetData());

    PixelData moved(std::move(copy));
    CHECK(copy.GetData() == nullptr);
    REQUIRE(moved.OwnsBuffer());
    uint8_t* released = moved.ReleaseBuffer();
    REQUIRE(released != nullptr);
    CHECK_FALSE(moved.OwnsBuffer());
    delete[] released;

    PixelData adopted(1, 1, 1, TextureFormat::RGBA8);
    adopted.SetOwnedBuffer(new uint8_t[adopted.GetSize()]{});
    uint8_t* adoptedPointer = adopted.GetData();
    adopted.SetOwnedBuffer(adoptedPointer);
    adopted.SetBuffer(adoptedPointer);
    CHECK(adopted.GetData() == adoptedPointer);
    CHECK(adopted.OwnsBuffer());
}

TEST_CASE("PixelData::CheckedColorAccess", "[PixelUtils]")
{
    PixelData pixels(2, 2, 1, TextureFormat::RGBA8);
    glm::vec4 color(1.0f);
    CHECK_FALSE(pixels.TryGetColorAt(0, 0, 0, color));
    pixels.AllocateInternalBuffer();
    CHECK_FALSE(pixels.TryGetColorAt(2, 0, 0, color));
    CHECK_FALSE(pixels.TrySetColorAt(0, 2, 0, color));

    PixelData compressed(4, 4, 1, TextureFormat::BC1);
    compressed.AllocateInternalBuffer();
    CHECK_FALSE(compressed.TryGetColorAt(0, 0, 0, color));
    CHECK_FALSE(compressed.TrySetColorAt(0, 0, 0, color));
}

TEST_CASE("PixelData::PaddedViewDerivesSlicePitch", "[PixelUtils]")
{
    constexpr uint32_t rowPitch = 12;
    std::array<uint8_t, rowPitch * 2> storage{};
    Ref<PixelData> view = PixelData::CreateView(2, 2, 1, TextureFormat::RGBA8, storage.data(), rowPitch);
    REQUIRE(view->IsValid());
    CHECK(view->GetRowPitch() == rowPitch);
    CHECK(view->GetSlicePitch() == rowPitch * 2);
}

TEST_CASE("PixelData::BoundStorageFreezesPitchLayout", "[PixelUtils]")
{
    PixelData pixels(2, 2, 1, TextureFormat::RGBA8);
    pixels.AllocateInternalBuffer();
    REQUIRE(pixels.IsValid());
    REQUIRE(pixels.GetSize() == 16);

    CHECK_FALSE(pixels.SetRowPitch(64));
    CHECK_FALSE(pixels.SetSlicePitch(128));
    CHECK(pixels.GetRowPitch() == 8);
    CHECK(pixels.GetSlicePitch() == 16);
    CHECK(pixels.GetSize() == 16);

    const glm::vec4 expected(0.25f, 0.5f, 0.75f, 1.0f);
    REQUIRE(pixels.TrySetColorAt(1, 1, 0, expected));
    PixelData copy(pixels);
    REQUIRE(copy.IsValid());
    CHECK(copy.GetSize() == 16);
    CHECK_THAT(copy.GetColorAt(1, 1).r, Catch::Matchers::WithinAbs(expected.r, 0.01f));
    CHECK_THAT(copy.GetColorAt(1, 1).g, Catch::Matchers::WithinAbs(expected.g, 0.01f));
    CHECK_THAT(copy.GetColorAt(1, 1).b, Catch::Matchers::WithinAbs(expected.b, 0.01f));
}

TEST_CASE("PixelData::InvalidLayoutsRejectStorage", "[PixelUtils]")
{
    std::array<uint8_t, 32> storage{};

    PixelData invalidView(2, 2, 1, TextureFormat::RGBA8);
    REQUIRE(invalidView.SetRowPitch(7));
    REQUIRE(invalidView.SetSlicePitch(14));
    CHECK_FALSE(invalidView.SetBuffer(storage.data()));
    CHECK(invalidView.GetData() == nullptr);
    CHECK_FALSE(invalidView.IsValid());

    uint8_t* ownedStorage = new uint8_t[storage.size()]{};
    CHECK_FALSE(invalidView.SetOwnedBuffer(ownedStorage));
    CHECK(invalidView.GetData() == nullptr);
    CHECK_FALSE(invalidView.OwnsBuffer());
    delete[] ownedStorage;

    PixelData invalidSlice(2, 2, 1, TextureFormat::RGBA8);
    REQUIRE(invalidSlice.SetSlicePitch(15));
    CHECK_FALSE(invalidSlice.SetBuffer(storage.data()));
    CHECK(invalidSlice.GetData() == nullptr);

    PixelData invalidCopy(invalidView);
    CHECK(invalidCopy.GetData() == nullptr);
    CHECK_FALSE(invalidCopy.OwnsBuffer());
    CHECK_FALSE(invalidCopy.IsValid());

    PixelData unbound(2, 2, 1, TextureFormat::RGBA8);
    REQUIRE(unbound.HasValidPitches());
    PixelData unboundCopy(unbound);
    CHECK(unboundCopy.GetData() == nullptr);
    CHECK_FALSE(unboundCopy.OwnsBuffer());

    Ref<PixelData> padded = PixelData::CreateView(2, 2, 1, TextureFormat::RGBA8, storage.data(), 12, 28);
    REQUIRE(padded->IsValid());
    CHECK(padded->GetRowPitch() == 12);
    CHECK(padded->GetSlicePitch() == 28);

    Ref<PixelData> compressed = PixelData::CreateView(5, 7, 1, TextureFormat::BC1, storage.data(), 16, 32);
    REQUIRE(compressed->IsValid());
    CHECK(compressed->GetPhysicalRowCount() == 2);
}

TEST_CASE("PixelUtils::Conversion::OverlappingLayouts", "[PixelUtils]")
{
    constexpr uint32_t sourceRowPitch = 8;
    constexpr uint32_t destinationRowPitch = 12;
    std::array<uint8_t, destinationRowPitch * 2> storage{};
    Ref<PixelData> source = PixelData::CreateView(2, 2, 1, TextureFormat::RGBA8, storage.data(), sourceRowPitch, sourceRowPitch * 2);
    Ref<PixelData> destination = PixelData::CreateView(2, 2, 1, TextureFormat::RGBA8, storage.data(), destinationRowPitch, destinationRowPitch * 2);

    REQUIRE(source->TrySetColorAt(0, 0, 0, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)));
    REQUIRE(source->TrySetColorAt(1, 0, 0, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)));
    REQUIRE(source->TrySetColorAt(0, 1, 0, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)));
    REQUIRE(source->TrySetColorAt(1, 1, 0, glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)));

    REQUIRE(PixelUtils::ConvertPixels(*source, *destination));
    CHECK_THAT(destination->GetColorAt(0, 1).b, Catch::Matchers::WithinRel(1.0f, 0.01f));
    CHECK_THAT(destination->GetColorAt(1, 1).r, Catch::Matchers::WithinRel(1.0f, 0.01f));
    CHECK_THAT(destination->GetColorAt(1, 1).g, Catch::Matchers::WithinRel(1.0f, 0.01f));
}

TEST_CASE("PixelUtils::UnpackAllowsIgnoredChannels", "[PixelUtils]")
{
    const std::array<uint8_t, 4> pixel{ 255, 64, 128, 32 };
    float red = 0.0f;
    float alpha = 0.0f;
    PixelUtils::UnpackPixel(&red, nullptr, nullptr, &alpha, TextureFormat::RGBA8, pixel.data());
    CHECK_THAT(red, Catch::Matchers::WithinRel(1.0f, 0.01f));
    CHECK_THAT(alpha, Catch::Matchers::WithinAbs(32.0f / 255.0f, 0.01f));
}

TEST_CASE("PixelUtils::IntegerPackingSanitizesNaN", "[PixelUtils]")
{
    std::array<uint8_t, 4> normalized{ 0xFF, 0xFF, 0xFF, 0xFF };
    PixelUtils::PackPixel(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 1.0f, TextureFormat::RGBA8, normalized.data());
    CHECK(normalized[0] == 0);

    std::array<uint8_t, 4> integer{ 0xFF, 0xFF, 0xFF, 0xFF };
    PixelUtils::PackPixel(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 1.0f, TextureFormat::R32I, integer.data());
    CHECK((integer == std::array<uint8_t, 4>{}));
}

TEST_CASE("PixelUtils::PackUnpack::SignedInteger", "[PixelUtils]")
{
    std::array<uint8_t, 4> pixel{};
    PixelUtils::PackPixel(-42.0f, 0.0f, 0.0f, 1.0f, TextureFormat::R32I, pixel.data());

    float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
    PixelUtils::UnpackPixel(&r, &g, &b, &a, TextureFormat::R32I, pixel.data());
    CHECK(r == -42.0f);
    CHECK(g == 0.0f);
    CHECK(b == 0.0f);
    CHECK(a == 1.0f);
}

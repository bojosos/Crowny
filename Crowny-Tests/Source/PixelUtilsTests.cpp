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

TEST_CASE("PixelUtils::GetNumBytes", "[PixelUtils]")
{
    CHECK(PixelUtils::GetNumBytes(TextureFormat::R8)              == 1);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::RG8)             == 2);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::RGB8)            == 3);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::RGBA8)           == 4);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::BGRA8)           == 4);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::RG16F)           == 4);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::RGBA16F)         == 8);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::RGB32F)          == 12);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::RGBA32F)         == 16);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::RG32F)           == 8);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::R32I)            == 4);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::DEPTH32F)        == 4);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::DEPTH24STENCIL8) == 4);
    // Compressed formats have no per-pixel byte count
    CHECK(PixelUtils::GetNumBytes(TextureFormat::BC1)  == 0);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::BC3)  == 0);
    CHECK(PixelUtils::GetNumBytes(TextureFormat::BC7)  == 0);
}

TEST_CASE("PixelUtils::GetBlockSize", "[PixelUtils]")
{
    // BC1/BC1a/BC4 = 8 bytes per 4x4 block
    CHECK(PixelUtils::GetBlockSize(TextureFormat::BC1)  == 8);
    CHECK(PixelUtils::GetBlockSize(TextureFormat::BC1a) == 8);
    CHECK(PixelUtils::GetBlockSize(TextureFormat::BC4)  == 8);
    // BC2/BC3/BC5/BC6H/BC7 = 16 bytes per 4x4 block
    CHECK(PixelUtils::GetBlockSize(TextureFormat::BC2)  == 16);
    CHECK(PixelUtils::GetBlockSize(TextureFormat::BC3)  == 16);
    CHECK(PixelUtils::GetBlockSize(TextureFormat::BC5)  == 16);
    CHECK(PixelUtils::GetBlockSize(TextureFormat::BC6H) == 16);
    CHECK(PixelUtils::GetBlockSize(TextureFormat::BC7)  == 16);
    // Uncompressed formats return per-pixel byte count
    CHECK(PixelUtils::GetBlockSize(TextureFormat::RGBA8) == 4);
    CHECK(PixelUtils::GetBlockSize(TextureFormat::R8)    == 1);
}

TEST_CASE("PixelUtils::GetMemSize", "[PixelUtils]")
{
    CHECK(PixelUtils::GetMemSize(4, 4, 1, TextureFormat::RGBA8)  == 64);
    CHECK(PixelUtils::GetMemSize(1, 1, 1, TextureFormat::RGBA32F) == 16);
    CHECK(PixelUtils::GetMemSize(8, 8, 1, TextureFormat::R8)     == 64);
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
    CHECK(rgba[0] == 8); CHECK(rgba[1] == 8); CHECK(rgba[2] == 8); CHECK(rgba[3] == 8);

    PixelUtils::GetBitDepths(TextureFormat::RGB8, rgba);
    CHECK(rgba[0] == 8); CHECK(rgba[1] == 8); CHECK(rgba[2] == 8); CHECK(rgba[3] == 0);

    PixelUtils::GetBitDepths(TextureFormat::R8, rgba);
    CHECK(rgba[0] == 8); CHECK(rgba[1] == 0); CHECK(rgba[2] == 0); CHECK(rgba[3] == 0);

    PixelUtils::GetBitDepths(TextureFormat::RGBA32F, rgba);
    CHECK(rgba[0] == 32); CHECK(rgba[1] == 32); CHECK(rgba[2] == 32); CHECK(rgba[3] == 32);

    PixelUtils::GetBitDepths(TextureFormat::RGB32F, rgba);
    CHECK(rgba[0] == 32); CHECK(rgba[1] == 32); CHECK(rgba[2] == 32); CHECK(rgba[3] == 0);

    PixelUtils::GetBitDepths(TextureFormat::RGBA16F, rgba);
    CHECK(rgba[0] == 16); CHECK(rgba[1] == 16); CHECK(rgba[2] == 16); CHECK(rgba[3] == 16);

    PixelUtils::GetBitDepths(TextureFormat::DEPTH24STENCIL8, rgba);
    CHECK(rgba[0] == 24); CHECK(rgba[1] == 8); CHECK(rgba[2] == 0); CHECK(rgba[3] == 0);
}

TEST_CASE("PixelUtils::GetMipSizeForLevel", "[PixelUtils]")
{
    uint32_t w, h, d;

    PixelUtils::GetMipSizeForLevel(256, 128, 1, 0, w, h, d);
    CHECK(w == 256); CHECK(h == 128); CHECK(d == 1);

    PixelUtils::GetMipSizeForLevel(256, 128, 1, 1, w, h, d);
    CHECK(w == 128); CHECK(h == 64); CHECK(d == 1);

    PixelUtils::GetMipSizeForLevel(256, 128, 1, 2, w, h, d);
    CHECK(w == 64); CHECK(h == 32); CHECK(d == 1);

    // Dimension clamps at 1
    PixelUtils::GetMipSizeForLevel(4, 1, 1, 3, w, h, d);
    CHECK(w == 1); CHECK(h == 1); CHECK(d == 1);
}

TEST_CASE("PixelUtils::D24S8 stencil mask", "[PixelUtils]")
{
    // Verify depth and stencil are packed/unpacked independently without overlap
    uint8_t pixel[4] = {};

    // Full depth (all 24 bits), zero stencil
    PixelUtils::PackPixel(1.0f, 0.0f, 0.0f, 1.0f, TextureFormat::DEPTH24STENCIL8, pixel);
    float depth, stencil, b, a;
    PixelUtils::UnpackPixel(&depth, &stencil, &b, &a, TextureFormat::DEPTH24STENCIL8, pixel);
    CHECK_THAT(depth,   Catch::Matchers::WithinAbs(1.0f, 0.001f));
    CHECK_THAT(stencil, Catch::Matchers::WithinAbs(0.0f, 0.001f));

    // Zero depth, full stencil (all 8 bits)
    std::memset(pixel, 0, 4);
    PixelUtils::PackPixel(0.0f, 1.0f, 0.0f, 1.0f, TextureFormat::DEPTH24STENCIL8, pixel);
    PixelUtils::UnpackPixel(&depth, &stencil, &b, &a, TextureFormat::DEPTH24STENCIL8, pixel);
    CHECK_THAT(depth,   Catch::Matchers::WithinAbs(0.0f, 0.001f));
    CHECK_THAT(stencil, Catch::Matchers::WithinAbs(1.0f, 0.001f));
}

TEST_CASE("PixelData::RGB8 ColorAt", "[PixelUtils]")
{
    PixelData pd(4, 4, 1, TextureFormat::RGB8);
    pd.AllocateInternalBuffer();

    glm::vec4 color(0.5f, 0.25f, 1.0f, 1.0f);
    pd.SetColorAt(2, 3, color);

    glm::vec4 read = pd.GetColorAt(2, 3);
    CHECK_THAT(read.r, Catch::Matchers::WithinRel(0.5f,  0.01f));
    CHECK_THAT(read.g, Catch::Matchers::WithinRel(0.25f, 0.01f));
    CHECK_THAT(read.b, Catch::Matchers::WithinRel(1.0f,  0.01f));

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
    CHECK(PixelData(8, 8, 1, TextureFormat::RGBA8).GetSize()   == 8 * 8 * 4);
    CHECK(PixelData(4, 4, 1, TextureFormat::RGB8).GetSize()    == 4 * 4 * 3);
    CHECK(PixelData(4, 4, 1, TextureFormat::R8).GetSize()      == 4 * 4 * 1);
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
    PixelUtils::ConvertPixels(src, dst);

    glm::vec4 c = dst.GetColorAt(0, 0);
    CHECK_THAT(c.r, Catch::Matchers::WithinRel(1.0f,  0.01f));
    CHECK_THAT(c.g, Catch::Matchers::WithinRel(0.5f,  0.01f));
    CHECK_THAT(c.b, Catch::Matchers::WithinRel(0.25f, 0.01f));
    CHECK_THAT(c.a, Catch::Matchers::WithinRel(1.0f,  0.01f));
}

TEST_CASE("PixelUtils::Conversion::RGBA8toRGBA32F", "[PixelUtils]")
{
    PixelData src(1, 1, 1, TextureFormat::RGBA8);
    src.AllocateInternalBuffer();
    src.SetColorAt(0, 0, glm::vec4(1.0f, 0.0f, 0.5f, 1.0f));

    PixelData dst(1, 1, 1, TextureFormat::RGBA32F);
    dst.AllocateInternalBuffer();
    PixelUtils::ConvertPixels(src, dst);

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
    PixelUtils::ConvertPixels(src, dst);

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

    // Should log an error and not crash
    PixelUtils::ConvertPixels(src, dst);
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

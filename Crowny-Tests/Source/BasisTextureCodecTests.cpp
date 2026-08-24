#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/BasisTextureCodec.h"

using namespace Crowny;

namespace
{
    Ref<PixelData> MakeSourceTexture()
    {
        Ref<PixelData> pixels = PixelData::Create(8, 8, 1, TextureFormat::RGBA8);
        for (uint32_t y = 0; y < 8; y++)
        {
            for (uint32_t x = 0; x < 8; x++)
            {
                pixels->SetColorAt(x, y, glm::vec4(static_cast<float>(x) / 7.0f, static_cast<float>(y) / 7.0f,
                                                   static_cast<float>(x ^ y) / 7.0f,
                                                   static_cast<float>(x + y) / 14.0f));
            }
        }
        return pixels;
    }
} // namespace

TEST_CASE("Basis texture targets follow hardware compression support", "[Renderer][Texture][Basis]")
{
    BasisTextureInfo opaque;
    opaque.HasAlpha = false;
    BasisTextureInfo alpha;
    alpha.HasAlpha = true;

    RenderCapabilities capabilities;
    capabilities.SetCapability(CW_TEXTURE_COMPRESSION_BC);
    CHECK(BasisTextureCodec::SelectTarget(opaque, TextureFormat::RGB8, capabilities) == TextureFormat::BC1);
    CHECK(BasisTextureCodec::SelectTarget(alpha, TextureFormat::RGBA8, capabilities) == TextureFormat::BC3);
    CHECK(BasisTextureCodec::SelectTarget(opaque, TextureFormat::R8, capabilities) == TextureFormat::BC4);
    CHECK(BasisTextureCodec::SelectTarget(opaque, TextureFormat::RG8, capabilities) == TextureFormat::BC5);

    capabilities = {};
    capabilities.SetCapability(CW_TEXTURE_COMPRESSION_BC);
    capabilities.SetCapability(CW_TEXTURE_COMPRESSION_BPTC);
    alpha.DiskFormat = TextureDiskFormat::UASTC;
    CHECK(BasisTextureCodec::SelectTarget(alpha, TextureFormat::RGBA8, capabilities) == TextureFormat::BC7);

    capabilities = {};
    capabilities.SetCapability(CW_TEXTURE_COMPRESSION_ASTC);
    CHECK(BasisTextureCodec::SelectTarget(alpha, TextureFormat::RGBA8, capabilities) == TextureFormat::ASTC4x4);

    capabilities = {};
    capabilities.SetCapability(CW_TEXTURE_COMPRESSION_ETC2);
    CHECK(BasisTextureCodec::SelectTarget(opaque, TextureFormat::RGB8, capabilities) == TextureFormat::ETC2_RGB);
    CHECK(BasisTextureCodec::SelectTarget(alpha, TextureFormat::RGBA8, capabilities) == TextureFormat::ETC2_RGBA);
    CHECK(BasisTextureCodec::SelectTarget(opaque, TextureFormat::RG8, capabilities) == TextureFormat::ETC2_RG11);

    capabilities = {};
    CHECK(BasisTextureCodec::SelectTarget(alpha, TextureFormat::RGBA8, capabilities) == TextureFormat::RGBA8);
    CHECK(BasisTextureCodec::SelectTarget(opaque, TextureFormat::R8, capabilities) == TextureFormat::R8);
}

TEST_CASE("Basis KTX2 payloads retain mips and transcode to GPU blocks", "[Renderer][Texture][Basis]")
{
    const Ref<PixelData> source = MakeSourceTexture();
    for (TextureDiskFormat diskFormat : { TextureDiskFormat::ETC1S, TextureDiskFormat::UASTC })
    {
        Vector<uint8_t> encoded;
        BasisTextureInfo info;
        String error;
        REQUIRE(BasisTextureCodec::Encode(*source, diskFormat, true, true, encoded, &info, &error));
        INFO(error);
        CHECK_FALSE(encoded.empty());
        CHECK(info.Width == 8);
        CHECK(info.Height == 8);
        CHECK(info.Levels == 4);
        CHECK(info.Faces == 1);
        CHECK(info.Components == 4);
        CHECK(info.HasAlpha);
        CHECK(info.SRGB);
        CHECK(info.DiskFormat == diskFormat);

        BasisTextureTranscodeResult blocks;
        REQUIRE(BasisTextureCodec::Transcode(encoded.data(), encoded.size(), TextureFormat::RGBA8, TextureFormat::BC3,
                                             0, blocks, &error));
        INFO(error);
        REQUIRE(blocks.Subresources.size() == 4);
        CHECK(blocks.Format == TextureFormat::BC3);
        CHECK(blocks.Subresources[0]->GetSize() == 64);
        CHECK(blocks.Subresources[1]->GetSize() == 16);
        CHECK(blocks.Subresources[2]->GetSize() == 16);
        CHECK(blocks.Subresources[3]->GetSize() == 16);

        BasisTextureTranscodeResult rgba;
        REQUIRE(BasisTextureCodec::Transcode(encoded.data(), encoded.size(), TextureFormat::RGBA8, TextureFormat::RGBA8,
                                             2, rgba, &error));
        INFO(error);
        REQUIRE(rgba.Subresources.size() == 2);
        CHECK(rgba.Subresources[0]->GetSize() == 8 * 8 * 4);
        CHECK(rgba.Subresources[1]->GetSize() == 4 * 4 * 4);
    }
}

TEST_CASE("Basis KTX2 preserves validated authored mip chains", "[Renderer][Texture][Basis][Mips]")
{
    Vector<Ref<PixelData>> authored;
    for (uint32_t mip = 0; mip < 4; ++mip)
    {
        const uint32_t dimension = 8u >> mip;
        Ref<PixelData> pixels = PixelData::Create(dimension, dimension, 1, TextureFormat::RGBA8);
        const glm::vec4 color(static_cast<float>(mip) / 3.0f, 1.0f - static_cast<float>(mip) / 3.0f,
                              0.25f * static_cast<float>(mip), 1.0f);
        for (uint32_t y = 0; y < dimension; ++y)
        {
            for (uint32_t x = 0; x < dimension; ++x)
                pixels->SetColorAt(x, y, color);
        }
        authored.push_back(std::move(pixels));
    }

    Vector<uint8_t> encoded;
    BasisTextureInfo info;
    String error;
    REQUIRE(BasisTextureCodec::Encode(authored, TextureDiskFormat::UASTC, false, encoded, &info, &error));
    INFO(error);
    CHECK(info.Levels == 4);

    BasisTextureTranscodeResult decoded;
    REQUIRE(BasisTextureCodec::Transcode(encoded.data(), encoded.size(), TextureFormat::RGBA8,
                                         TextureFormat::RGBA8, 0, decoded, &error));
    INFO(error);
    REQUIRE(decoded.Subresources.size() == 4);
    for (uint32_t mip = 0; mip < decoded.Subresources.size(); ++mip)
    {
        const uint32_t dimension = 8u >> mip;
        CHECK(decoded.Subresources[mip]->GetWidth() == dimension);
        CHECK(decoded.Subresources[mip]->GetHeight() == dimension);
        CHECK(decoded.Subresources[mip]->GetSize() == dimension * dimension * 4u);
    }

    Vector<Ref<PixelData>> invalid = authored;
    invalid[2] = PixelData::Create(3, 3, 1, TextureFormat::RGBA8);
    CHECK_FALSE(BasisTextureCodec::Encode(invalid, TextureDiskFormat::UASTC, false, encoded, nullptr, &error));
}

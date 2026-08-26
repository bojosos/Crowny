#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/BasisTextureCodec.h"

using namespace Crowny;

namespace
{
    Ref<PixelData> MakeSourceTexture(uint32_t width = 8, uint32_t height = 8,
                                     const glm::vec4* solidColor = nullptr)
    {
        Ref<PixelData> pixels = PixelData::Create(width, height, 1, TextureFormat::RGBA8);
        for (uint32_t y = 0; y < height; y++)
        {
            for (uint32_t x = 0; x < width; x++)
            {
                const float widthScale = static_cast<float>(width > 1 ? width - 1u : 1u);
                const float heightScale = static_cast<float>(height > 1 ? height - 1u : 1u);
                const uint32_t largestDimension = std::max(width, height);
                const float largestScale = static_cast<float>(largestDimension > 1 ? largestDimension - 1u : 1u);
                const uint64_t combinedExtent = static_cast<uint64_t>(width) + height;
                const float extentScale = static_cast<float>(combinedExtent > 2 ? combinedExtent - 2u : 1u);
                const glm::vec4 color = solidColor != nullptr
                                          ? *solidColor
                                          : glm::vec4(static_cast<float>(x) / widthScale,
                                                      static_cast<float>(y) / heightScale,
                                                      static_cast<float>(x ^ y) / largestScale,
                                                      static_cast<float>(x + y) / extentScale);
                pixels->SetColorAt(x, y, color);
            }
        }
        return pixels;
    }

    void WriteLittleEndian32(Vector<uint8_t>& bytes, size_t offset, uint32_t value)
    {
        REQUIRE(offset + sizeof(value) <= bytes.size());
        bytes[offset + 0] = static_cast<uint8_t>(value);
        bytes[offset + 1] = static_cast<uint8_t>(value >> 8u);
        bytes[offset + 2] = static_cast<uint8_t>(value >> 16u);
        bytes[offset + 3] = static_cast<uint8_t>(value >> 24u);
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
        CHECK(info.Layers == 1);
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
        CHECK(blocks.Subresources[0].Pixels->GetSize() == 64);
        CHECK(blocks.Subresources[1].Pixels->GetSize() == 16);
        CHECK(blocks.Subresources[2].Pixels->GetSize() == 16);
        CHECK(blocks.Subresources[3].Pixels->GetSize() == 16);

        BasisTextureTranscodeResult rgba;
        REQUIRE(BasisTextureCodec::Transcode(encoded.data(), encoded.size(), TextureFormat::RGBA8, TextureFormat::RGBA8,
                                             2, rgba, &error));
        INFO(error);
        REQUIRE(rgba.Subresources.size() == 2);
        CHECK(rgba.Subresources[0].Pixels->GetSize() == 8 * 8 * 4);
        CHECK(rgba.Subresources[1].Pixels->GetSize() == 4 * 4 * 4);
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
        CHECK(decoded.Subresources[mip].MipLevel == mip);
        CHECK(decoded.Subresources[mip].Layer == 0);
        CHECK(decoded.Subresources[mip].Face == 0);
        CHECK(decoded.Subresources[mip].Pixels->GetWidth() == dimension);
        CHECK(decoded.Subresources[mip].Pixels->GetHeight() == dimension);
        CHECK(decoded.Subresources[mip].Pixels->GetSize() == dimension * dimension * 4u);
    }

    Vector<Ref<PixelData>> invalid = authored;
    invalid[2] = PixelData::Create(3, 3, 1, TextureFormat::RGBA8);
    CHECK_FALSE(BasisTextureCodec::Encode(invalid, TextureDiskFormat::UASTC, false, encoded, nullptr, &error));
}

TEST_CASE("Basis KTX2 texture arrays retain layer face and mip identity", "[Renderer][Texture][Basis][Array]")
{
    const std::array<glm::vec4, 4> colors = {
        glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
        glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)
    };
    BasisTextureSource source;
    source.Layers = 2;
    source.Faces = 1;
    source.Levels = 2;
    source.Subresources = {
        MakeSourceTexture(8, 8, &colors[0]), MakeSourceTexture(8, 8, &colors[1]),
        MakeSourceTexture(4, 4, &colors[2]), MakeSourceTexture(4, 4, &colors[3])
    };

    Vector<uint8_t> encoded;
    BasisTextureInfo info;
    String error;
    REQUIRE(BasisTextureCodec::Encode(source, TextureDiskFormat::ETC1S, false, encoded, &info, &error));
    INFO(error);
    CHECK(info.Layers == 2);
    CHECK(info.Faces == 1);
    CHECK(info.Levels == 2);
    CHECK(info.GetSliceCount() == 2);
    CHECK(info.GetSliceIndex(1, 0) == 1);
    CHECK(info.GetRuntimeShape() == TextureShape::TEXTURE_2D);

    BasisTextureTranscodeResult decoded;
    REQUIRE(BasisTextureCodec::Transcode(encoded.data(), encoded.size(), TextureFormat::RGBA8,
                                         TextureFormat::RGBA8, 0, decoded, &error));
    INFO(error);
    REQUIRE(decoded.Subresources.size() == 4);
    for (uint32_t mip = 0; mip < 2; mip++)
    {
        for (uint32_t layer = 0; layer < 2; layer++)
        {
            const size_t index = static_cast<size_t>(mip) * 2u + layer;
            CHECK(decoded.Subresources[index].MipLevel == mip);
            CHECK(decoded.Subresources[index].Layer == layer);
            CHECK(decoded.Subresources[index].Face == 0);
            const glm::vec4 decodedColor = decoded.Subresources[index].Pixels->GetColorAt(0, 0);
            const glm::vec4 expected = colors[index];
            CHECK(glm::distance(glm::vec3(decodedColor), glm::vec3(expected)) < 0.2f);
        }
    }
}

TEST_CASE("Basis KTX2 cube arrays retain six faces per layer", "[Renderer][Texture][Basis][Array][Cube]")
{
    BasisTextureSource source;
    source.Layers = 2;
    source.Faces = 6;
    source.Levels = 2;
    source.Subresources.reserve(24);
    for (uint32_t mip = 0; mip < source.Levels; mip++)
    {
        const uint32_t dimension = 8u >> mip;
        for (uint32_t layer = 0; layer < source.Layers; layer++)
        {
            for (uint32_t face = 0; face < source.Faces; face++)
            {
                const glm::vec4 color(static_cast<float>(face) / 5.0f, static_cast<float>(layer),
                                      static_cast<float>(mip), 1.0f);
                source.Subresources.push_back(MakeSourceTexture(dimension, dimension, &color));
            }
        }
    }

    Vector<uint8_t> encoded;
    BasisTextureInfo info;
    String error;
    REQUIRE(BasisTextureCodec::Encode(source, TextureDiskFormat::UASTC, false, encoded, &info, &error));
    INFO(error);
    CHECK(info.Layers == 2);
    CHECK(info.Faces == 6);
    CHECK(info.Levels == 2);
    CHECK(info.GetSliceCount() == 12);
    CHECK(info.GetSliceIndex(1, 5) == 11);
    CHECK(info.GetRuntimeShape() == TextureShape::TEXTURE_CUBE);

    BasisTextureTranscodeResult decoded;
    REQUIRE(BasisTextureCodec::Transcode(encoded.data(), encoded.size(), TextureFormat::RGBA8,
                                         TextureFormat::BC3, 0, decoded, &error));
    INFO(error);
    REQUIRE(decoded.Subresources.size() == 24);
    CHECK(decoded.Subresources[0].MipLevel == 0);
    CHECK(decoded.Subresources[0].Layer == 0);
    CHECK(decoded.Subresources[0].Face == 0);
    CHECK(decoded.Subresources[11].MipLevel == 0);
    CHECK(decoded.Subresources[11].Layer == 1);
    CHECK(decoded.Subresources[11].Face == 5);
    CHECK(decoded.Subresources[12].MipLevel == 1);
    CHECK(decoded.Subresources[12].Layer == 0);
    CHECK(decoded.Subresources[12].Face == 0);
}

TEST_CASE("Basis KTX2 rejects unsafe array ranges", "[Renderer][Texture][Basis][Array]")
{
    const Ref<PixelData> source = MakeSourceTexture(4, 4);
    Vector<uint8_t> encoded;
    String error;

    BasisTextureSource incomplete;
    incomplete.Layers = 2;
    incomplete.Subresources = { source };
    CHECK_FALSE(BasisTextureCodec::Encode(incomplete, TextureDiskFormat::UASTC, false, encoded, nullptr, &error));

    BasisTextureSource nonSquareCube;
    nonSquareCube.Faces = 6;
    nonSquareCube.Subresources.assign(6, MakeSourceTexture(4, 2));
    CHECK_FALSE(BasisTextureCodec::Encode(nonSquareCube, TextureDiskFormat::UASTC, false, encoded, nullptr, &error));

    REQUIRE(BasisTextureCodec::Encode(*source, TextureDiskFormat::UASTC, false, false, encoded, nullptr, &error));

    // KTX2 layerCount follows the 12-byte identifier and five uint32 header fields.
    Vector<uint8_t> impossibleLayers = encoded;
    WriteLittleEndian32(impossibleLayers, 32, std::numeric_limits<uint32_t>::max());
    BasisTextureInfo info;
    CHECK_FALSE(BasisTextureCodec::Inspect(impossibleLayers.data(), impossibleLayers.size(), info, &error));
    CHECK(error.find("subresource") != String::npos);

    Vector<uint8_t> invalidFaces = encoded;
    WriteLittleEndian32(invalidFaces, 36, 5);
    CHECK_FALSE(BasisTextureCodec::Inspect(invalidFaces.data(), invalidFaces.size(), info, &error));

    Vector<uint8_t> impossibleLevels = encoded;
    WriteLittleEndian32(impossibleLevels, 40, 32);
    CHECK_FALSE(BasisTextureCodec::Inspect(impossibleLevels.data(), impossibleLevels.size(), info, &error));

    Vector<uint8_t> truncated(encoded.begin(), encoded.begin() + encoded.size() / 2u);
    BasisTextureTranscodeResult decoded;
    CHECK_FALSE(BasisTextureCodec::Transcode(truncated.data(), truncated.size(), TextureFormat::RGBA8,
                                             TextureFormat::RGBA8, 0, decoded, &error));
    CHECK(decoded.Subresources.empty());
}

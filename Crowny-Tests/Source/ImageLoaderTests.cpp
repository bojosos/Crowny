#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Crowny/Import/ImageLoader.h"
#include "Crowny/Renderer/BasisTextureCodec.h"

#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <future>

using namespace Crowny;

namespace
{
    Ref<PixelData> MakeSolidTexture(uint32_t width, uint32_t height, const glm::vec4& color)
    {
        Ref<PixelData> pixels = PixelData::Create(width, height, 1, TextureFormat::RGBA8);
        for (uint32_t y = 0; y < height; y++)
        {
            for (uint32_t x = 0; x < width; x++)
                pixels->SetColorAt(x, y, color);
        }
        return pixels;
    }

    class TemporaryImageFile
    {
    public:
        TemporaryImageFile(StringView extension, const uint8_t* data, size_t size)
        {
            const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
            m_Path = std::filesystem::temp_directory_path() / ("crowny_image_" + std::to_string(unique) + "." + String(extension));
            std::ofstream stream(m_Path, std::ios::binary);
            stream.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        }

        ~TemporaryImageFile()
        {
            std::error_code error;
            std::filesystem::remove(m_Path, error);
        }

        const Path& GetPath() const { return m_Path; }

    private:
        Path m_Path;
    };
} // namespace

TEST_CASE("Image loader probes raster metadata without decoding pixels", "[Assets][Importer][Image]")
{
    const std::array<uint8_t, 17> ppm = { 'P', '6', '\n', '1', ' ', '2', '\n', '2', '5', '5', '\n', 255, 0, 0, 0, 0, 255 };

    ImageLoadOptions options;
    options.MetadataOnly = true;
    const ImageLoadResult result = ImageLoader::Load(ImageLoadRequest::FromMemory(ppm.data(), ppm.size(), options));

    REQUIRE(result);
    CHECK(result.Status == ImageLoadStatus::Succeeded);
    CHECK(result.Info.Container == ImageContainerFormat::Raster);
    CHECK(result.Info.FileFormat == ImageFileFormat::PNM);
    CHECK(result.Info.Width == 1);
    CHECK(result.Info.Height == 2);
    CHECK(result.Info.Channels == 3);
    CHECK(result.Info.BitDepth == 8);
    CHECK(result.Info.PixelFormat == TextureFormat::RGB8);
    CHECK(result.Pixels == nullptr);
    CHECK(result.Subresources.empty());
    CHECK(result.SourceData.empty());
}

TEST_CASE("Image loader applies orientation without global stb state", "[Assets][Importer][Image]")
{
    const std::array<uint8_t, 17> ppm = { 'P', '6', '\n', '1', ' ', '2', '\n', '2', '5', '5', '\n', 255, 0, 0, 0, 0, 255 };

    ImageLoadResult topLeft = ImageLoader::DecodeMemory(ppm.data(), ppm.size());
    REQUIRE(topLeft);
    REQUIRE(topLeft.Pixels != nullptr);
    CHECK(topLeft.Info.Orientation == ImageOrientation::TopLeft);
    CHECK(topLeft.Pixels->GetColorAt(0, 0).r > 0.99f);
    CHECK(topLeft.Pixels->GetColorAt(0, 0).b < 0.01f);

    ImageLoadOptions options;
    options.FlipVertically = true;
    ImageLoadResult bottomLeft = ImageLoader::DecodeMemory(ppm.data(), ppm.size(), options);
    REQUIRE(bottomLeft);
    REQUIRE(bottomLeft.Pixels != nullptr);
    CHECK(bottomLeft.Info.Orientation == ImageOrientation::BottomLeft);
    CHECK(bottomLeft.Pixels->GetColorAt(0, 0).r < 0.01f);
    CHECK(bottomLeft.Pixels->GetColorAt(0, 0).b > 0.99f);
}

TEST_CASE("Image loader decodes raster sources concurrently", "[Assets][Importer][Image]")
{
    const std::array<uint8_t, 17> ppm = { 'P', '6', '\n', '1', ' ', '2', '\n', '2', '5', '5', '\n', 255, 0, 0, 0, 0, 255 };
    std::array<std::future<ImageLoadResult>, 8> decodes;
    for (size_t index = 0; index < decodes.size(); index++)
    {
        decodes[index] = std::async(std::launch::async, [&, index]() {
            ImageLoadOptions options;
            options.FlipVertically = index % 2u != 0;
            return ImageLoader::DecodeMemory(ppm.data(), ppm.size(), options);
        });
    }

    for (size_t index = 0; index < decodes.size(); index++)
    {
        const ImageLoadResult result = decodes[index].get();
        REQUIRE(result);
        REQUIRE(result.Pixels != nullptr);
        const glm::vec4 firstPixel = result.Pixels->GetColorAt(0, 0);
        if (index % 2u == 0)
            CHECK(firstPixel.r > 0.99f);
        else
            CHECK(firstPixel.b > 0.99f);
    }
}

TEST_CASE("Image loader preserves 16-bit grayscale precision", "[Assets][Importer][Image]")
{
    const std::array<uint8_t, 15> pgm = { 'P', '5', '\n', '1', ' ', '1', '\n', '6', '5', '5', '3', '5', '\n', 0x80, 0x00 };

    const ImageLoadResult result = ImageLoader::DecodeMemory(pgm.data(), pgm.size());
    REQUIRE(result);
    REQUIRE(result.Pixels != nullptr);
    CHECK(result.Info.BitDepth == 16);
    CHECK_FALSE(result.Info.IsFloat);
    CHECK(result.Info.PixelFormat == TextureFormat::R16);
    CHECK_THAT(result.Pixels->GetColorAt(0, 0).r, Catch::Matchers::WithinAbs(32768.0f / 65535.0f, 0.0001f));
}

TEST_CASE("Image loader preserves 16-bit PNM channel semantics", "[Assets][Importer][Image]")
{
    const std::array<uint8_t, 19> ppm = { 'P', '6', '\n', '1', ' ', '1', '\n', '6', '5', '5', '3', '5', '\n', 0xFF, 0xFF, 0x80, 0x00, 0x00, 0x00 };

    const ImageLoadResult precise = ImageLoader::DecodeMemory(ppm.data(), ppm.size());
    REQUIRE(precise);
    REQUIRE(precise.Pixels != nullptr);
    CHECK(precise.Info.Channels == 3);
    CHECK(precise.Info.BitDepth == 16);
    CHECK_FALSE(precise.Info.IsFloat);
    CHECK(precise.Info.PixelFormat == TextureFormat::RGBA16);
    CHECK(PixelUtils::GetComponentCount(precise.Info.PixelFormat) == 4);
    const glm::vec4 preciseColor = precise.Pixels->GetColorAt(0, 0);
    CHECK_THAT(preciseColor.r, Catch::Matchers::WithinAbs(1.0f, 0.0001f));
    CHECK_THAT(preciseColor.g, Catch::Matchers::WithinAbs(32768.0f / 65535.0f, 0.0001f));
    CHECK_THAT(preciseColor.b, Catch::Matchers::WithinAbs(0.0f, 0.0001f));
    CHECK_THAT(preciseColor.a, Catch::Matchers::WithinAbs(1.0f, 0.0001f));

    ImageLoadOptions byteOptions;
    byteOptions.Preserve16Bit = false;
    const ImageLoadResult bytes = ImageLoader::DecodeMemory(ppm.data(), ppm.size(), byteOptions);
    REQUIRE(bytes);
    REQUIRE(bytes.Pixels != nullptr);
    CHECK(bytes.Info.BitDepth == 16);
    CHECK_FALSE(bytes.Info.IsFloat);
    CHECK(bytes.Info.PixelFormat == TextureFormat::RGB8);
    const glm::vec4 byteColor = bytes.Pixels->GetColorAt(0, 0);
    CHECK_THAT(byteColor.r, Catch::Matchers::WithinAbs(1.0f, 1.0f / 255.0f));
    CHECK_THAT(byteColor.g, Catch::Matchers::WithinAbs(32768.0f / 65535.0f, 1.0f / 255.0f));
    CHECK_THAT(byteColor.b, Catch::Matchers::WithinAbs(0.0f, 1.0f / 255.0f));
}

TEST_CASE("Image loader preserves grayscale alpha semantics", "[Assets][Importer][Image]")
{
    const std::array<uint8_t, 68> png = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x04, 0x00, 0x00, 0x00, 0xB5, 0x1C, 0x0C, 0x02, 0x00, 0x00,
        0x00, 0x0B, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x68, 0x70, 0x00, 0x00, 0x01, 0x43, 0x00, 0xC1, 0x5F,
        0xD4, 0x95, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
    };

    const ImageLoadResult result = ImageLoader::DecodeMemory(png.data(), png.size());
    REQUIRE(result);
    REQUIRE(result.Pixels != nullptr);
    CHECK(result.Info.FileFormat == ImageFileFormat::PNG);
    CHECK(result.Info.Channels == 2);
    CHECK(result.Info.ChannelLayout == ImageChannelLayout::GrayAlpha);
    CHECK(result.Info.HasAlpha);
    CHECK(result.Info.PixelFormat == TextureFormat::RG8);
    REQUIRE(result.Subresources.size() == 1);
    const glm::vec4 stored = result.Pixels->GetColorAt(0, 0);
    CHECK_THAT(stored.r, Catch::Matchers::WithinAbs(128.0f / 255.0f, 1.0f / 255.0f));
    CHECK_THAT(stored.g, Catch::Matchers::WithinAbs(64.0f / 255.0f, 1.0f / 255.0f));
}

TEST_CASE("Image loader enforces source dimension and decoded memory limits", "[Assets][Importer][Image]")
{
    const std::array<uint8_t, 17> ppm = { 'P', '6', '\n', '1', ' ', '2', '\n', '2', '5', '5', '\n', 255, 0, 0, 0, 0, 255 };

    ImageLoadOptions sourceOptions;
    sourceOptions.MaximumSourceBytes = ppm.size() - 1u;
    const ImageLoadResult sourceLimited = ImageLoader::DecodeMemory(ppm.data(), ppm.size(), sourceOptions);
    REQUIRE_FALSE(sourceLimited);
    REQUIRE(sourceLimited.Diagnostics.size() == 1);
    CHECK(sourceLimited.Diagnostics[0].Code == ImageDiagnosticCode::SourceTooLarge);

    ImageLoadOptions memoryOptions;
    memoryOptions.MaximumDecodedBytes = 5;
    const ImageLoadResult memoryLimited = ImageLoader::DecodeMemory(ppm.data(), ppm.size(), memoryOptions);
    REQUIRE_FALSE(memoryLimited);
    REQUIRE(memoryLimited.Diagnostics.size() == 1);
    CHECK(memoryLimited.Diagnostics[0].Code == ImageDiagnosticCode::DecodedImageTooLarge);

    const std::array<uint8_t, 14> largeHeader = { 'P', '6', '\n', '2', '0', '4', '8', ' ', '1', '\n', '2', '5', '5', '\n' };
    ImageLoadOptions dimensionOptions;
    dimensionOptions.MaximumDimension = 1024;
    const ImageLoadResult dimensionLimited = ImageLoader::ProbeMemory(largeHeader.data(), largeHeader.size(), dimensionOptions);
    REQUIRE_FALSE(dimensionLimited);
    REQUIRE(dimensionLimited.Diagnostics.size() == 1);
    CHECK(dimensionLimited.Diagnostics[0].Code == ImageDiagnosticCode::DimensionsTooLarge);
}

TEST_CASE("Image loader honors cancellation and reports corrupt data", "[Assets][Importer][Image]")
{
    const std::array<uint8_t, 4> invalid = { 1, 2, 3, 4 };
    const ImageLoadResult corrupt = ImageLoader::ProbeMemory(invalid.data(), invalid.size());
    CHECK_FALSE(corrupt);
    CHECK(corrupt.Status == ImageLoadStatus::Failed);
    CHECK_FALSE(corrupt.Error.empty());
    REQUIRE(corrupt.Diagnostics.size() == 1);
    CHECK(corrupt.Diagnostics[0].Code == ImageDiagnosticCode::UnsupportedFormat);
    CHECK(corrupt.Diagnostics[0].Stage == ImageLoadStage::Probe);
    CHECK(corrupt.Diagnostics[0].Message == corrupt.Error);

    std::atomic<bool> canceled{ true };
    ImageLoadOptions options;
    options.Cancellation = &canceled;
    const ImageLoadResult canceledResult = ImageLoader::DecodeMemory(invalid.data(), invalid.size(), options);
    CHECK_FALSE(canceledResult);
    CHECK(canceledResult.Status == ImageLoadStatus::Canceled);
    CHECK(canceledResult.Canceled);
    CHECK(canceledResult.Error.empty());
    REQUIRE(canceledResult.Diagnostics.size() == 1);
    CHECK(canceledResult.Diagnostics[0].Code == ImageDiagnosticCode::Canceled);
    CHECK(canceledResult.Diagnostics[0].Stage == ImageLoadStage::Source);
}

TEST_CASE("Image loader retains KTX2 source data for file import requests", "[Assets][Importer][Image][Basis]")
{
    Ref<PixelData> source = PixelData::Create(4, 4, 1, TextureFormat::RGBA8);
    REQUIRE(source != nullptr);
    for (uint32_t y = 0; y < source->GetHeight(); y++)
    {
        for (uint32_t x = 0; x < source->GetWidth(); x++)
            source->SetColorAt(x, y, glm::vec4(static_cast<float>(x) / 3.0f, static_cast<float>(y) / 3.0f, 0.5f, 1.0f));
    }

    Vector<uint8_t> encoded;
    BasisTextureInfo basisInfo;
    String error;
    REQUIRE(BasisTextureCodec::Encode(*source, TextureDiskFormat::UASTC, false, false, encoded, &basisInfo, &error));
    INFO(error);
    const TemporaryImageFile file("ktx2", encoded.data(), encoded.size());

    ImageLoadOptions options;
    options.DecodePixels = false;
    const ImageLoadResult result = ImageLoader::Load(ImageLoadRequest::FromFile(file.GetPath(), options));

    REQUIRE(result);
    CHECK(result.Status == ImageLoadStatus::Succeeded);
    CHECK(result.Info.Container == ImageContainerFormat::KTX2);
    CHECK(result.Info.FileFormat == ImageFileFormat::KTX2);
    CHECK(result.Info.Width == 4);
    CHECK(result.Info.Height == 4);
    CHECK(result.Info.Layers == 1);
    CHECK(result.Info.DiskFormat == TextureDiskFormat::UASTC);
    CHECK(result.Pixels == nullptr);
    CHECK(result.SourceData == encoded);
}

TEST_CASE("Image loader reports KTX2 array topology without decoding", "[Assets][Importer][Image][Basis][Array]")
{
    BasisTextureSource source;
    source.Layers = 2;
    source.Faces = 1;
    source.Levels = 2;
    source.Subresources = {
        MakeSolidTexture(4, 4, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)),
        MakeSolidTexture(4, 4, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)),
        MakeSolidTexture(2, 2, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)),
        MakeSolidTexture(2, 2, glm::vec4(1.0f, 1.0f, 0.0f, 1.0f))
    };

    Vector<uint8_t> encoded;
    String error;
    REQUIRE(BasisTextureCodec::Encode(source, TextureDiskFormat::UASTC, false, encoded, nullptr, &error));
    INFO(error);

    const ImageLoadResult result = ImageLoader::ProbeMemory(encoded.data(), encoded.size());
    REQUIRE(result);
    CHECK(result.Info.Container == ImageContainerFormat::KTX2);
    CHECK(result.Info.Width == 4);
    CHECK(result.Info.Height == 4);
    CHECK(result.Info.Layers == 2);
    CHECK(result.Info.Faces == 1);
    CHECK(result.Info.MipLevels == 2);
    CHECK(result.Info.GetRuntimeShape() == TextureShape::TEXTURE_2D);
    CHECK(result.Pixels == nullptr);

    ImageLoadOptions exactLimitOptions;
    exactLimitOptions.MaximumDecodedBytes = 160;
    const ImageLoadResult decoded = ImageLoader::DecodeMemory(encoded.data(), encoded.size(), exactLimitOptions);
    REQUIRE(decoded);
    REQUIRE(decoded.Pixels != nullptr);
    CHECK(decoded.Info.MipLevels == 2);
    CHECK_FALSE(decoded.Info.IsCompressed);
    REQUIRE(decoded.Subresources.size() == 4);
    CHECK(decoded.Subresources[0].MipLevel == 0);
    CHECK(decoded.Subresources[0].Layer == 0);
    CHECK(decoded.Subresources[1].MipLevel == 0);
    CHECK(decoded.Subresources[1].Layer == 1);
    CHECK(decoded.Subresources[2].MipLevel == 1);
    CHECK(decoded.Subresources[2].Layer == 0);
    CHECK(decoded.Subresources[2].Pixels->GetWidth() == 2);
    CHECK(decoded.Subresources[2].Pixels->GetHeight() == 2);
    CHECK(decoded.Subresources[3].MipLevel == 1);
    CHECK(decoded.Subresources[3].Layer == 1);

    ImageLoadOptions insufficientLimitOptions;
    insufficientLimitOptions.MaximumDecodedBytes = 159;
    const ImageLoadResult limited = ImageLoader::DecodeMemory(encoded.data(), encoded.size(), insufficientLimitOptions);
    REQUIRE_FALSE(limited);
    REQUIRE(limited.Diagnostics.size() == 1);
    CHECK(limited.Diagnostics[0].Code == ImageDiagnosticCode::DecodedImageTooLarge);
}

TEST_CASE("Image loader recognizes common image signatures", "[Assets][Importer][Image]")
{
    const std::array<uint8_t, 8> png = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
    const std::array<uint8_t, 3> jpeg = { 0xFF, 0xD8, 0xFF };
    const std::array<uint8_t, 6> gif = { 'G', 'I', 'F', '8', '9', 'a' };
    CHECK(ImageLoader::SupportsSignature(png.data(), png.size()));
    CHECK(ImageLoader::SupportsSignature(jpeg.data(), jpeg.size()));
    CHECK(ImageLoader::SupportsSignature(gif.data(), gif.size()));
}

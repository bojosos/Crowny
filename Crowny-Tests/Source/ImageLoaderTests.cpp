#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Crowny/Import/ImageLoader.h"
#include "Crowny/Renderer/BasisTextureCodec.h"

#include <array>
#include <atomic>
#include <chrono>
#include <fstream>

using namespace Crowny;

namespace
{
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

TEST_CASE("Image loader preserves 16-bit grayscale precision", "[Assets][Importer][Image]")
{
    const std::array<uint8_t, 15> pgm = { 'P', '5', '\n', '1', ' ', '1', '\n', '6', '5', '5', '3', '5', '\n', 0x80, 0x00 };

    const ImageLoadResult result = ImageLoader::DecodeMemory(pgm.data(), pgm.size());
    REQUIRE(result);
    REQUIRE(result.Pixels != nullptr);
    CHECK(result.Info.BitDepth == 16);
    CHECK(result.Info.IsFloat);
    CHECK(result.Info.PixelFormat == TextureFormat::R32F);
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
    CHECK(precise.Info.IsFloat);
    CHECK(precise.Info.PixelFormat == TextureFormat::RGB32F);
    const glm::vec4 preciseColor = precise.Pixels->GetColorAt(0, 0);
    CHECK_THAT(preciseColor.r, Catch::Matchers::WithinAbs(1.0f, 0.0001f));
    CHECK_THAT(preciseColor.g, Catch::Matchers::WithinAbs(32768.0f / 65535.0f, 0.0001f));
    CHECK_THAT(preciseColor.b, Catch::Matchers::WithinAbs(0.0f, 0.0001f));

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
    CHECK(result.Info.DiskFormat == TextureDiskFormat::UASTC);
    CHECK(result.Pixels == nullptr);
    CHECK(result.SourceData == encoded);
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

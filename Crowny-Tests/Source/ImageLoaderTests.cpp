#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Crowny/Import/ImageLoader.h"

#include <array>
#include <atomic>

using namespace Crowny;

TEST_CASE("Image loader probes raster metadata without decoding pixels", "[Assets][Importer][Image]")
{
    const std::array<uint8_t, 17> ppm = { 'P', '6', '\n', '1', ' ', '2', '\n', '2', '5', '5', '\n',
                                          255, 0,   0,    0,   0,   255 };

    ImageLoadOptions options;
    options.MetadataOnly = true;
    const ImageLoadResult result = ImageLoader::ProbeMemory(ppm.data(), ppm.size(), options);

    REQUIRE(result);
    CHECK(result.Info.Container == ImageContainerFormat::Raster);
    CHECK(result.Info.Width == 1);
    CHECK(result.Info.Height == 2);
    CHECK(result.Info.Channels == 3);
    CHECK(result.Info.BitDepth == 8);
    CHECK(result.Info.PixelFormat == TextureFormat::RGB8);
    CHECK(result.Pixels == nullptr);
}

TEST_CASE("Image loader applies orientation without global stb state", "[Assets][Importer][Image]")
{
    const std::array<uint8_t, 17> ppm = { 'P', '6', '\n', '1', ' ', '2', '\n', '2', '5', '5', '\n',
                                          255, 0,   0,    0,   0,   255 };

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
    const std::array<uint8_t, 19> ppm = { 'P',  '6',  '\n', '1',  ' ',  '1',  '\n', '6',  '5',  '5',
                                          '3',  '5',  '\n', 0xFF, 0xFF, 0x80, 0x00, 0x00, 0x00 };

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
    CHECK_FALSE(corrupt.Error.empty());

    std::atomic<bool> canceled{ true };
    ImageLoadOptions options;
    options.Cancellation = &canceled;
    const ImageLoadResult canceledResult = ImageLoader::DecodeMemory(invalid.data(), invalid.size(), options);
    CHECK_FALSE(canceledResult);
    CHECK(canceledResult.Canceled);
    CHECK(canceledResult.Error.empty());
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

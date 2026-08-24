#include "RenderTestImage.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>

namespace Crowny::RenderTests
{
    namespace
    {
        uint16_t ReadU16(std::istream& stream)
        {
            uint8_t bytes[2]{};
            stream.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
            return static_cast<uint16_t>(bytes[0]) | static_cast<uint16_t>(bytes[1] << 8u);
        }

        uint32_t ReadU32(std::istream& stream)
        {
            uint8_t bytes[4]{};
            stream.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
            return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8u) | (static_cast<uint32_t>(bytes[2]) << 16u) |
                   (static_cast<uint32_t>(bytes[3]) << 24u);
        }

        void WriteU16(std::ostream& stream, uint16_t value)
        {
            const uint8_t bytes[] = { static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8u) };
            stream.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
        }

        void WriteU32(std::ostream& stream, uint32_t value)
        {
            const uint8_t bytes[] = { static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8u), static_cast<uint8_t>(value >> 16u),
                                      static_cast<uint8_t>(value >> 24u) };
            stream.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
        }
    } // namespace

    Image::Image(uint32_t width, uint32_t height) : Width(width), Height(height), Pixels(static_cast<size_t>(width) * height * 4u, 0u) {}

    bool Image::IsValid() const { return Width > 0 && Height > 0 && Pixels.size() == static_cast<size_t>(Width) * Height * 4u; }

    uint8_t* Image::Pixel(uint32_t x, uint32_t y) { return Pixels.data() + (static_cast<size_t>(y) * Width + x) * 4u; }

    const uint8_t* Image::Pixel(uint32_t x, uint32_t y) const { return Pixels.data() + (static_cast<size_t>(y) * Width + x) * 4u; }

    bool LoadBmp(const Path& path, Image& image, String& error)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            error = "Could not open " + path.string();
            return false;
        }

        if (ReadU16(stream) != 0x4D42u)
        {
            error = "Not a BMP file: " + path.string();
            return false;
        }
        (void)ReadU32(stream);
        (void)ReadU16(stream);
        (void)ReadU16(stream);
        const uint32_t pixelOffset = ReadU32(stream);
        const uint32_t dibSize = ReadU32(stream);
        if (dibSize < 40u)
        {
            error = "Unsupported BMP header in " + path.string();
            return false;
        }

        const int32_t width = static_cast<int32_t>(ReadU32(stream));
        const int32_t signedHeight = static_cast<int32_t>(ReadU32(stream));
        const uint16_t planes = ReadU16(stream);
        const uint16_t bitsPerPixel = ReadU16(stream);
        const uint32_t compression = ReadU32(stream);
        if (width <= 0 || signedHeight == 0 || planes != 1u || compression != 0u || (bitsPerPixel != 24u && bitsPerPixel != 32u))
        {
            error = "Only uncompressed 24-bit and 32-bit BMP files are supported: " + path.string();
            return false;
        }

        const uint32_t height = static_cast<uint32_t>(signedHeight < 0 ? -static_cast<int64_t>(signedHeight) : signedHeight);
        const uint32_t rowBytes = ((static_cast<uint32_t>(width) * bitsPerPixel + 31u) / 32u) * 4u;
        Vector<uint8_t> row(rowBytes);
        Image loaded(static_cast<uint32_t>(width), height);
        stream.seekg(pixelOffset, std::ios::beg);
        for (uint32_t fileY = 0; fileY < height; ++fileY)
        {
            stream.read(reinterpret_cast<char*>(row.data()), row.size());
            if (!stream)
            {
                error = "Truncated BMP pixel data in " + path.string();
                return false;
            }
            const uint32_t y = signedHeight > 0 ? height - 1u - fileY : fileY;
            for (uint32_t x = 0; x < loaded.Width; ++x)
            {
                const uint8_t* source = row.data() + static_cast<size_t>(x) * (bitsPerPixel / 8u);
                uint8_t* destination = loaded.Pixel(x, y);
                destination[0] = source[2];
                destination[1] = source[1];
                destination[2] = source[0];
                destination[3] = bitsPerPixel == 32u ? source[3] : 255u;
            }
        }
        image = std::move(loaded);
        return true;
    }

    bool SaveBmp(const Path& path, const Image& image, String& error)
    {
        if (!image.IsValid())
        {
            error = "Cannot save an invalid image to " + path.string();
            return false;
        }
        if (!path.parent_path().empty())
            fs::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "Could not create " + path.string();
            return false;
        }

        constexpr uint32_t pixelOffset = 14u + 40u;
        const uint32_t pixelBytes = image.Width * image.Height * 4u;
        WriteU16(stream, 0x4D42u);
        WriteU32(stream, pixelOffset + pixelBytes);
        WriteU16(stream, 0u);
        WriteU16(stream, 0u);
        WriteU32(stream, pixelOffset);
        WriteU32(stream, 40u);
        WriteU32(stream, image.Width);
        WriteU32(stream, image.Height);
        WriteU16(stream, 1u);
        WriteU16(stream, 32u);
        WriteU32(stream, 0u);
        WriteU32(stream, pixelBytes);
        WriteU32(stream, 2835u);
        WriteU32(stream, 2835u);
        WriteU32(stream, 0u);
        WriteU32(stream, 0u);

        Vector<uint8_t> row(static_cast<size_t>(image.Width) * 4u);
        for (uint32_t y = image.Height; y-- > 0;)
        {
            for (uint32_t x = 0; x < image.Width; ++x)
            {
                const uint8_t* source = image.Pixel(x, y);
                uint8_t* destination = row.data() + static_cast<size_t>(x) * 4u;
                destination[0] = source[2];
                destination[1] = source[1];
                destination[2] = source[0];
                destination[3] = source[3];
            }
            stream.write(reinterpret_cast<const char*>(row.data()), row.size());
        }
        if (!stream)
        {
            error = "Failed while writing " + path.string();
            return false;
        }
        return true;
    }

    Comparison Compare(const Image& expected, const Image& actual, const Tolerance& tolerance)
    {
        Comparison result;
        if (!expected.IsValid() || !actual.IsValid())
        {
            result.Message = "Expected and actual images must both be valid";
            return result;
        }
        if (expected.Width != actual.Width || expected.Height != actual.Height)
        {
            result.Message = "Image dimensions differ: expected " + std::to_string(expected.Width) + "x" + std::to_string(expected.Height) +
                             ", actual " + std::to_string(actual.Width) + "x" + std::to_string(actual.Height);
            return result;
        }

        const uint32_t channelCount = tolerance.CompareAlpha ? 4u : 3u;
        double absoluteError = 0.0;
        double squaredError = 0.0;
        result.ComparedPixels = static_cast<uint64_t>(expected.Width) * expected.Height;
        for (uint64_t pixel = 0; pixel < result.ComparedPixels; ++pixel)
        {
            uint8_t pixelError = 0;
            for (uint32_t channel = 0; channel < channelCount; ++channel)
            {
                const size_t index = static_cast<size_t>(pixel) * 4u + channel;
                const uint8_t error =
                  static_cast<uint8_t>(std::abs(static_cast<int32_t>(expected.Pixels[index]) - static_cast<int32_t>(actual.Pixels[index])));
                pixelError = std::max(pixelError, error);
                result.MaxChannelError = std::max(result.MaxChannelError, error);
                absoluteError += error;
                squaredError += static_cast<double>(error) * error;
            }
            if (pixelError > tolerance.PixelThreshold)
                ++result.FailingPixels;
        }

        const double sampleCount = static_cast<double>(result.ComparedPixels) * channelCount;
        result.MeanAbsoluteError = absoluteError / sampleCount;
        result.RootMeanSquareError = std::sqrt(squaredError / sampleCount);
        result.FailingPixelRatio = static_cast<double>(result.FailingPixels) / result.ComparedPixels;
        result.Passed = result.MaxChannelError <= tolerance.MaxChannelError && result.MeanAbsoluteError <= tolerance.MaxMeanAbsoluteError &&
                        result.FailingPixelRatio <= tolerance.MaxFailingPixelRatio;
        if (!result.Passed)
        {
            result.Message = "max error " + std::to_string(result.MaxChannelError) + ", mean absolute error " +
                             std::to_string(result.MeanAbsoluteError) + ", failing pixels " + std::to_string(result.FailingPixels) + "/" +
                             std::to_string(result.ComparedPixels);
        }
        return result;
    }

    Image BuildDiff(const Image& expected, const Image& actual)
    {
        if (!expected.IsValid() || !actual.IsValid() || expected.Width != actual.Width || expected.Height != actual.Height)
            return {};
        Image diff(expected.Width, expected.Height);
        for (size_t index = 0; index < diff.Pixels.size(); index += 4u)
        {
            for (uint32_t channel = 0; channel < 3u; ++channel)
            {
                const uint32_t error = static_cast<uint32_t>(
                  std::abs(static_cast<int32_t>(expected.Pixels[index + channel]) - static_cast<int32_t>(actual.Pixels[index + channel])));
                diff.Pixels[index + channel] = static_cast<uint8_t>(std::min(error * 4u, 255u));
            }
            diff.Pixels[index + 3u] = 255u;
        }
        return diff;
    }
} // namespace Crowny::RenderTests

#pragma once

#include "Crowny/Common/StdHeaders.h"

namespace Crowny::RenderTests
{
    struct Image
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        Vector<uint8_t> Pixels;

        Image() = default;
        Image(uint32_t width, uint32_t height);

        bool IsValid() const;
        uint8_t* Pixel(uint32_t x, uint32_t y);
        const uint8_t* Pixel(uint32_t x, uint32_t y) const;
    };

    struct Tolerance
    {
        uint8_t PixelThreshold = 0;
        uint8_t MaxChannelError = 0;
        double MaxMeanAbsoluteError = 0.0;
        double MaxFailingPixelRatio = 0.0;
        bool CompareAlpha = true;
    };

    struct Comparison
    {
        bool Passed = false;
        uint8_t MaxChannelError = 0;
        double MeanAbsoluteError = 0.0;
        double RootMeanSquareError = 0.0;
        double FailingPixelRatio = 0.0;
        uint64_t FailingPixels = 0;
        uint64_t ComparedPixels = 0;
        String Message;
    };

    bool LoadBmp(const Path& path, Image& image, String& error);
    bool SaveBmp(const Path& path, const Image& image, String& error);
    Comparison Compare(const Image& expected, const Image& actual, const Tolerance& tolerance);
    Image BuildDiff(const Image& expected, const Image& actual);
} // namespace Crowny::RenderTests

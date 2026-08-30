#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

#include <glm/glm.hpp>

namespace Crowny
{
    struct ViewportTextureExtent
    {
        uint32_t Width = 0u;
        uint32_t Height = 0u;

        bool operator==(const ViewportTextureExtent&) const = default;
    };

    struct ViewportPickPixel
    {
        uint32_t X = 0u;
        uint32_t Y = 0u;

        bool operator==(const ViewportPickPixel&) const = default;
    };

    inline std::optional<ViewportTextureExtent> ResolveViewportTextureExtent(const glm::vec2& displaySize)
    {
        if (!std::isfinite(displaySize.x) || !std::isfinite(displaySize.y) || displaySize.x <= 0.0f || displaySize.y <= 0.0f)
            return std::nullopt;

        const double width = std::floor(static_cast<double>(displaySize.x));
        const double height = std::floor(static_cast<double>(displaySize.y));
        constexpr double maximumExtent = static_cast<double>(std::numeric_limits<uint32_t>::max());
        if (width > maximumExtent || height > maximumExtent)
            return std::nullopt;

        return ViewportTextureExtent{ width < 1.0 ? 1u : static_cast<uint32_t>(width),
                                      height < 1.0 ? 1u : static_cast<uint32_t>(height) };
    }

    inline std::optional<ViewportPickPixel> ResolveViewportPickPixel(const glm::vec2& screenPosition, const glm::vec4& imageBounds,
                                                                     const ViewportTextureExtent& textureExtent)
    {
        if (textureExtent.Width == 0u || textureExtent.Height == 0u || !std::isfinite(screenPosition.x) ||
            !std::isfinite(screenPosition.y) || !std::isfinite(imageBounds.x) || !std::isfinite(imageBounds.y) ||
            !std::isfinite(imageBounds.z) || !std::isfinite(imageBounds.w))
            return std::nullopt;

        const double minimumX = static_cast<double>(imageBounds.x);
        const double minimumY = static_cast<double>(imageBounds.y);
        const double maximumX = static_cast<double>(imageBounds.z);
        const double maximumY = static_cast<double>(imageBounds.w);
        const double screenX = static_cast<double>(screenPosition.x);
        const double screenY = static_cast<double>(screenPosition.y);
        const double displayWidth = maximumX - minimumX;
        const double displayHeight = maximumY - minimumY;
        if (displayWidth <= 0.0 || displayHeight <= 0.0 || screenX < minimumX || screenX >= maximumX || screenY < minimumY ||
            screenY >= maximumY)
            return std::nullopt;

        const double textureX = (screenX - minimumX) / displayWidth * static_cast<double>(textureExtent.Width);
        const double textureYFromTop = (screenY - minimumY) / displayHeight * static_cast<double>(textureExtent.Height);
        if (textureX < 0.0 || textureX >= static_cast<double>(textureExtent.Width) || textureYFromTop < 0.0 ||
            textureYFromTop >= static_cast<double>(textureExtent.Height))
            return std::nullopt;

        const uint32_t x = static_cast<uint32_t>(textureX);
        const uint32_t yFromTop = static_cast<uint32_t>(textureYFromTop);
        return ViewportPickPixel{ x, textureExtent.Height - yFromTop - 1u };
    }
} // namespace Crowny

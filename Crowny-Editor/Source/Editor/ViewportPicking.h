#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

#include <glm/glm.hpp>

namespace Crowny
{
    // Intersect the cursor ray with the ground. Near the horizon, use the plane through
    // the camera's focus so drops remain within a useful editing distance.
    inline std::optional<glm::vec3> ResolveViewportDropPosition(const glm::vec2& screenPosition, const glm::vec4& imageBounds,
                                                                const glm::mat4& viewProjection, const glm::vec3& cameraPosition,
                                                                const glm::vec3& cameraForward, float focusDistance)
    {
        const glm::vec2 size(imageBounds.z - imageBounds.x, imageBounds.w - imageBounds.y);
        if (!(size.x > 0.0f && size.y > 0.0f) || !(screenPosition.x >= imageBounds.x && screenPosition.x < imageBounds.z &&
                                                   screenPosition.y >= imageBounds.y && screenPosition.y < imageBounds.w))
            return std::nullopt;

        const glm::vec2 uv = (screenPosition - glm::vec2(imageBounds)) / size;
        const glm::vec4 world = glm::inverse(viewProjection) * glm::vec4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0.0f, 1.0f);
        if (!std::isfinite(world.w) || std::abs(world.w) < 1e-7f)
            return std::nullopt;
        const glm::vec3 direction = glm::normalize(glm::vec3(world) / world.w - cameraPosition);
        const float forward = glm::dot(direction, cameraForward);
        if (!std::isfinite(forward) || forward <= 1e-5f || !std::isfinite(focusDistance) || focusDistance <= 0.0f)
            return std::nullopt;
        const float fallbackDistance = focusDistance / forward;
        float distance = fallbackDistance;
        if (std::abs(direction.y) > 1e-5f)
        {
            const float groundDistance = -cameraPosition.y / direction.y;
            if (groundDistance > 0.0f && groundDistance <= fallbackDistance * 10.0f)
                distance = groundDistance;
        }
        const glm::vec3 position = cameraPosition + direction * distance;
        if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
            return std::nullopt;
        return position;
    }

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

        return ViewportTextureExtent{ width < 1.0 ? 1u : static_cast<uint32_t>(width), height < 1.0 ? 1u : static_cast<uint32_t>(height) };
    }

    inline std::optional<ViewportPickPixel> ResolveViewportPickPixel(const glm::vec2& screenPosition, const glm::vec4& imageBounds,
                                                                     const ViewportTextureExtent& textureExtent)
    {
        if (textureExtent.Width == 0u || textureExtent.Height == 0u || !std::isfinite(screenPosition.x) || !std::isfinite(screenPosition.y) ||
            !std::isfinite(imageBounds.x) || !std::isfinite(imageBounds.y) || !std::isfinite(imageBounds.z) || !std::isfinite(imageBounds.w))
            return std::nullopt;

        const double minimumX = static_cast<double>(imageBounds.x);
        const double minimumY = static_cast<double>(imageBounds.y);
        const double maximumX = static_cast<double>(imageBounds.z);
        const double maximumY = static_cast<double>(imageBounds.w);
        const double screenX = static_cast<double>(screenPosition.x);
        const double screenY = static_cast<double>(screenPosition.y);
        const double displayWidth = maximumX - minimumX;
        const double displayHeight = maximumY - minimumY;
        if (displayWidth <= 0.0 || displayHeight <= 0.0 || screenX < minimumX || screenX >= maximumX || screenY < minimumY || screenY >= maximumY)
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

#pragma once

#include "Crowny/Common/StdHeaders.h"

namespace Crowny
{
    struct ProjectHubRect
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Width = 0.0f;
        float Height = 0.0f;
    };

    struct ProjectHubLayout
    {
        ProjectHubRect Card;
        ProjectHubRect Sidebar;
        ProjectHubRect Content;
    };

    constexpr ProjectHubLayout CalculateProjectHubLayout(float viewportWidth, float viewportHeight) noexcept
    {
        constexpr float outerMargin = 32.0f;
        constexpr float maximumWidth = 960.0f;
        constexpr float maximumHeight = 640.0f;
        constexpr float minimumSidebarWidth = 168.0f;
        constexpr float maximumSidebarWidth = 220.0f;
        constexpr float childGap = 8.0f;

        const float safeWidth = std::max(0.0f, viewportWidth);
        const float safeHeight = std::max(0.0f, viewportHeight);
        const float cardWidth = std::min(maximumWidth, std::max(0.0f, safeWidth - outerMargin * 2.0f));
        const float cardHeight = std::min(maximumHeight, std::max(0.0f, safeHeight - outerMargin * 2.0f));
        const float preferredSidebarWidth = std::clamp(cardWidth * 0.22f, minimumSidebarWidth, maximumSidebarWidth);
        const float sidebarWidth = std::min(preferredSidebarWidth, cardWidth);
        const float gap = std::min(childGap, std::max(0.0f, cardWidth - sidebarWidth));

        ProjectHubLayout layout;
        layout.Card = { (safeWidth - cardWidth) * 0.5f, (safeHeight - cardHeight) * 0.5f, cardWidth, cardHeight };
        layout.Sidebar = { 0.0f, 0.0f, sidebarWidth, cardHeight };
        layout.Content = { sidebarWidth + gap, 0.0f, std::max(0.0f, cardWidth - sidebarWidth - gap), cardHeight };
        return layout;
    }
} // namespace Crowny

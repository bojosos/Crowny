#pragma once

#include <algorithm>

namespace Crowny
{
    struct ConsoleSplitLayout
    {
        float MessageHeight;
        float SplitterHeight;
        float MinimumMessageHeight;
        float MaximumMessageHeight;
        bool DetailsVisible;
    };

    inline ConsoleSplitLayout BuildConsoleSplitLayout(float availableHeight, bool hasSelectedMessage, float rememberedMessageHeight)
    {
        const float safeHeight = std::max(0.0f, availableHeight);
        constexpr float minimumPaneHeight = 1.0f;

        if (!hasSelectedMessage || safeHeight < minimumPaneHeight * 2.0f + 1.0f)
            return { safeHeight, 0.0f, safeHeight, safeHeight, false };

        constexpr float preferredSplitterHeight = 6.0f;
        constexpr float preferredDetailsHeight = 100.0f;
        constexpr float preferredMinimumMessageHeight = 120.0f;
        const float splitterHeight = std::min(preferredSplitterHeight, safeHeight - minimumPaneHeight * 2.0f);
        const float paneHeight = safeHeight - splitterHeight;
        const float desiredDetailsHeight = std::min(preferredDetailsHeight, safeHeight * 0.4f);
        const float minimumDetailsHeight = std::clamp(desiredDetailsHeight, minimumPaneHeight, paneHeight - minimumPaneHeight);
        const float maximumMessageHeight = paneHeight - minimumDetailsHeight;
        const float minimumMessageHeight = std::min(preferredMinimumMessageHeight, maximumMessageHeight);
        const float messageHeight =
          rememberedMessageHeight <= 0.0f ? maximumMessageHeight : std::clamp(rememberedMessageHeight, minimumMessageHeight, maximumMessageHeight);
        return { messageHeight, splitterHeight, minimumMessageHeight, maximumMessageHeight, true };
    }
} // namespace Crowny

#pragma once

#include <algorithm>
#include <cstdint>

namespace Crowny
{
    enum class AssetBrowserToolbarMode : uint8_t
    {
        Wide,
        Compact,
        Narrow
    };

    struct AssetBrowserToolbarLayout
    {
        AssetBrowserToolbarMode Mode = AssetBrowserToolbarMode::Narrow;
        uint32_t ColumnCount = 2u;
        uint32_t ControlRowCount = 2u;
        bool SearchSharesControlRow = false;
        bool ShowsThumbnailSize = false;

        bool operator==(const AssetBrowserToolbarLayout&) const = default;
    };

    constexpr AssetBrowserToolbarLayout GetAssetBrowserToolbarLayout(float availableWidth, bool gridView)
    {
        constexpr float minimumSearchWidth = 220.0f;
        constexpr float typeWidth = 120.0f;
        constexpr float sortWidth = 108.0f;
        constexpr float viewWidth = 92.0f;
        constexpr float thumbnailWidth = 125.0f;
        constexpr float columnSpacing = 8.0f;

        const uint32_t controlCount = gridView ? 4u : 3u;
        const float fixedControlWidth = typeWidth + sortWidth + viewWidth + (gridView ? thumbnailWidth : 0.0f);
        const float wideMinimum = minimumSearchWidth + fixedControlWidth + columnSpacing * static_cast<float>(controlCount);
        if (availableWidth >= wideMinimum)
            return { AssetBrowserToolbarMode::Wide, controlCount + 1u, 1u, true, gridView };

        const float compactMinimum = gridView ? 430.0f : 330.0f;
        if (availableWidth >= compactMinimum)
            return { AssetBrowserToolbarMode::Compact, controlCount, 1u, false, gridView };

        return { AssetBrowserToolbarMode::Narrow, 2u, 2u, false, gridView };
    }

    constexpr float GetAssetBrowserNavigationWidth(float frameHeight, float reloadLabelWidth, float itemSpacing, float framePaddingX)
    {
        return frameHeight * 3.0f + reloadLabelWidth + framePaddingX * 2.0f + itemSpacing * 3.0f;
    }

    constexpr bool NeedsAssetBrowserBreadcrumbScrollbar(float contentWidth, float availableWidth)
    {
        return contentWidth > std::max(availableWidth, 0.0f);
    }

    struct AssetBrowserItemRange
    {
        uint32_t Begin = 0u;
        uint32_t End = 0u;

        bool operator==(const AssetBrowserItemRange&) const = default;
    };

    struct AssetBrowserPresentationFingerprint
    {
        uint64_t Identity = 0u;
        int64_t ModifiedTime = 0;
        uint64_t Revision = 0u;
        uint32_t ByteSize = 0u;
        bool IsFile = false;

        bool operator==(const AssetBrowserPresentationFingerprint&) const = default;
    };

    constexpr bool NeedsAssetBrowserPresentationRefresh(const AssetBrowserPresentationFingerprint& cached,
                                                         const AssetBrowserPresentationFingerprint& current)
    {
        return cached != current;
    }

    constexpr uint32_t GetAssetBrowserRowCount(uint32_t itemCount, uint32_t columnCount)
    {
        const uint32_t columns = std::max(columnCount, 1u);
        return itemCount / columns + (itemCount % columns != 0u ? 1u : 0u);
    }

    constexpr uint32_t GetAssetBrowserItemRow(uint32_t itemIndex, uint32_t columnCount) { return itemIndex / std::max(columnCount, 1u); }

    constexpr AssetBrowserItemRange GetAssetBrowserItemRange(uint32_t rowBegin, uint32_t rowEnd, uint32_t columnCount, uint32_t itemCount)
    {
        const uint32_t columns = std::max(columnCount, 1u);
        const uint64_t firstItem = static_cast<uint64_t>(rowBegin) * columns;
        const uint64_t endItem = static_cast<uint64_t>(std::max(rowEnd, rowBegin)) * columns;
        return { static_cast<uint32_t>(std::min<uint64_t>(firstItem, itemCount)), static_cast<uint32_t>(std::min<uint64_t>(endItem, itemCount)) };
    }
} // namespace Crowny

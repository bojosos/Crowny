#pragma once

#include <algorithm>
#include <cstdint>

namespace Crowny
{
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

#pragma once

#include "Crowny/Common/StdHeaders.h"

#include <optional>
#include <span>

namespace Crowny
{
    struct AssetBrowserSelectionResult
    {
        static constexpr uint32_t InvalidIndex = static_cast<uint32_t>(-1);

        uint32_t StartIndex = InvalidIndex;
        uint32_t EndIndex = 0;
        bool SelectionChanged = false;
    };

    struct AssetBrowserItemId
    {
        uint32_t UpperBits = 0;
        uint32_t LowerBits = 0;

        bool operator==(const AssetBrowserItemId&) const = default;
    };

    AssetBrowserItemId MakeAssetBrowserItemId(const Path& path);

    AssetBrowserSelectionResult ReconcileAssetBrowserSelection(UnorderedSet<Path, HashPath>& selection,
                                                               std::span<const Path* const> visibleEntryPaths,
                                                               std::span<const Path* const> sortedVisibleEntryPaths,
                                                               const std::optional<Path>& preferredStartPath = {},
                                                               const std::optional<Path>& preferredEndPath = {});
} // namespace Crowny

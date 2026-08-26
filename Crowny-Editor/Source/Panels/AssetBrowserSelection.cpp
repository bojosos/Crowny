#include "cwepch.h"

#include "Panels/AssetBrowserSelection.h"

namespace Crowny
{
    AssetBrowserItemId MakeAssetBrowserItemId(const Path& path)
    {
        const uint64_t hash = static_cast<uint64_t>(HashPath{}(path));
        return { static_cast<uint32_t>(hash >> 32u), static_cast<uint32_t>(hash & 0xffffffffu) };
    }

    AssetBrowserSelectionResult ReconcileAssetBrowserSelection(UnorderedSet<Path, HashPath>& selection,
                                                               std::span<const Path* const> visibleEntryPaths,
                                                               std::span<const Path* const> sortedVisibleEntryPaths,
                                                               const std::optional<Path>& preferredStartPath,
                                                               const std::optional<Path>& preferredEndPath)
    {
        AssetBrowserSelectionResult result;
        const size_t previousSelectionSize = selection.size();

        for (auto iter = selection.begin(); iter != selection.end();)
        {
            const auto visibleIter = std::lower_bound(sortedVisibleEntryPaths.begin(), sortedVisibleEntryPaths.end(), *iter,
                                                      [](const Path* lhs, const Path& rhs) { return *lhs < rhs; });
            if (visibleIter == sortedVisibleEntryPaths.end() || **visibleIter != *iter)
                iter = selection.erase(iter);
            else
                ++iter;
        }

        result.SelectionChanged = selection.size() != previousSelectionSize;
        if (selection.empty())
            return result;

        uint32_t firstSelectedIndex = AssetBrowserSelectionResult::InvalidIndex;
        uint32_t lastSelectedIndex = 0;
        for (size_t index = 0; index < visibleEntryPaths.size(); index++)
        {
            const Path& path = *visibleEntryPaths[index];
            if (!selection.contains(path))
                continue;

            if (firstSelectedIndex == AssetBrowserSelectionResult::InvalidIndex)
                firstSelectedIndex = static_cast<uint32_t>(index);
            lastSelectedIndex = static_cast<uint32_t>(index);

            if (preferredStartPath && path == *preferredStartPath)
                result.StartIndex = static_cast<uint32_t>(index);
            if (preferredEndPath && path == *preferredEndPath)
                result.EndIndex = static_cast<uint32_t>(index);
        }

        if (result.StartIndex == AssetBrowserSelectionResult::InvalidIndex)
            result.StartIndex = firstSelectedIndex;
        if (!preferredEndPath || !selection.contains(*preferredEndPath))
            result.EndIndex = lastSelectedIndex;

        return result;
    }
} // namespace Crowny

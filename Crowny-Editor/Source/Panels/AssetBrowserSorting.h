#pragma once

#include "Editor/AssetLibraryTypes.h"

#include <algorithm>
#include <cctype>

namespace Crowny
{
    inline bool AssetBrowserEntryNameLess(const LibraryEntry& left, const LibraryEntry& right)
    {
        if (left.Type != right.Type)
            return left.Type == LibraryEntryType::Directory;

        const size_t commonLength = std::min(left.ElementName.size(), right.ElementName.size());
        for (size_t index = 0; index < commonLength; index++)
        {
            const int leftCharacter = std::tolower(static_cast<unsigned char>(left.ElementName[index]));
            const int rightCharacter = std::tolower(static_cast<unsigned char>(right.ElementName[index]));
            if (leftCharacter != rightCharacter)
                return leftCharacter < rightCharacter;
        }
        if (left.ElementName.size() != right.ElementName.size())
            return left.ElementName.size() < right.ElementName.size();

        // Break case and duplicate-basename ties independently of filesystem enumeration order.
        if (left.ElementName != right.ElementName)
            return left.ElementName < right.ElementName;
        return left.Filepath < right.Filepath;
    }

    inline void SortAssetBrowserEntriesByName(Vector<Ref<LibraryEntry>>& entries)
    {
        std::sort(entries.begin(), entries.end(),
                  [](const Ref<LibraryEntry>& left, const Ref<LibraryEntry>& right) { return AssetBrowserEntryNameLess(*left, *right); });
    }
} // namespace Crowny

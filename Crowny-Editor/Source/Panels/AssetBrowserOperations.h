#pragma once

#include "Crowny/Common/StdHeaders.h"

#include <optional>

namespace Crowny
{
    struct AssetBrowserMoveOperation
    {
        Path Source;
        Path Destination;

        bool operator==(const AssetBrowserMoveOperation&) const = default;
    };

    class AssetBrowserOperationQueue
    {
    public:
        bool EnqueueMove(Path source, Path destination);
        Vector<AssetBrowserMoveOperation> TakePending();

        bool Empty() const { return m_Pending.empty(); }
        size_t Size() const { return m_Pending.size(); }

    private:
        Vector<AssetBrowserMoveOperation> m_Pending;
    };

    std::optional<Path> RemapAssetBrowserSelectionAfterMove(UnorderedSet<Path, HashPath>& selection,
                                                            const AssetBrowserMoveOperation& operation, bool moveSucceeded);
} // namespace Crowny

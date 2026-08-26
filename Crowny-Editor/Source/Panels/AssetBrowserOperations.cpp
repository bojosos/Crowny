#include "cwepch.h"

#include "Panels/AssetBrowserOperations.h"

namespace Crowny
{
    namespace
    {
        bool TryRemapMovedPath(const Path& selectedPath, const AssetBrowserMoveOperation& operation, Path& remappedPath)
        {
            if (operation.Source.empty() || operation.Destination.empty())
                return false;

            auto selectedComponent = selectedPath.begin();
            for (auto sourceComponent = operation.Source.begin(); sourceComponent != operation.Source.end(); ++sourceComponent)
            {
                if (selectedComponent == selectedPath.end() || *selectedComponent != *sourceComponent)
                    return false;
                ++selectedComponent;
            }

            remappedPath = operation.Destination;
            for (; selectedComponent != selectedPath.end(); ++selectedComponent)
                remappedPath /= *selectedComponent;
            return true;
        }
    } // namespace

    bool AssetBrowserOperationQueue::EnqueueMove(Path source, Path destination)
    {
        if (source.empty() || destination.empty() || source == destination)
            return false;

        m_Pending.push_back({ std::move(source), std::move(destination) });
        return true;
    }

    Vector<AssetBrowserMoveOperation> AssetBrowserOperationQueue::TakePending()
    {
        Vector<AssetBrowserMoveOperation> pending;
        pending.swap(m_Pending);
        return pending;
    }

    std::optional<Path> RemapAssetBrowserSelectionAfterMove(UnorderedSet<Path, HashPath>& selection,
                                                            const AssetBrowserMoveOperation& operation, bool moveSucceeded)
    {
        if (!moveSucceeded)
            return {};

        Vector<std::pair<Path, Path>> remappedPaths;
        remappedPaths.reserve(selection.size());
        std::optional<Path> preferredSource;
        std::optional<Path> preferredDestination;
        bool exactSourceSelected = false;
        for (const Path& selectedPath : selection)
        {
            Path remappedPath;
            if (!TryRemapMovedPath(selectedPath, operation, remappedPath))
                continue;

            remappedPaths.emplace_back(selectedPath, remappedPath);
            if (selectedPath == operation.Source)
            {
                exactSourceSelected = true;
                preferredSource = selectedPath;
                preferredDestination = remappedPath;
            }
            else if (!exactSourceSelected && (!preferredSource || selectedPath < *preferredSource))
            {
                preferredSource = selectedPath;
                preferredDestination = remappedPath;
            }
        }

        for (const auto& [selectedPath, remappedPath] : remappedPaths)
        {
            auto selectedNode = selection.extract(selectedPath);
            selectedNode.value() = remappedPath;
            selection.insert(std::move(selectedNode));
        }
        return preferredDestination;
    }
} // namespace Crowny

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
    Vector<AssetBrowserImportOperation> PlanAssetBrowserImports(const Vector<Path>& sources, const Path& destinationFolder,
                                                                const std::function<bool(const Path&)>& isDirectory,
                                                                const std::function<Path(const Path&)>& makeUnique)
    {
        Vector<AssetBrowserImportOperation> operations;
        if (destinationFolder.empty())
            return operations;

        const Path destination = destinationFolder.lexically_normal();
        const auto isPrefixOf = [](const Path& prefix, const Path& path) {
            auto prefixIt = prefix.begin();
            auto pathIt = path.begin();
            for (; prefixIt != prefix.end(); ++prefixIt, ++pathIt)
            {
                if (pathIt == path.end() || *pathIt != *prefixIt)
                    return false;
            }
            return true;
        };

        UnorderedSet<Path, HashPath> plannedSources;
        UnorderedSet<Path, HashPath> plannedDestinations;
        for (const Path& rawSource : sources)
        {
            if (rawSource.empty())
                continue;
            Path source = rawSource.lexically_normal();
            if (!source.has_filename())
                source = source.parent_path();
            const Path filename = source.filename();
            if (source.empty() || filename.empty() || filename == "." || filename == "..")
                continue;
            if (!plannedSources.insert(source).second)
                continue;

            // Already in this folder, or a folder that contains the destination (copying it would recurse forever).
            if (source.parent_path() == destination || source == destination)
                continue;
            const bool directory = isDirectory(source);
            if (directory && isPrefixOf(source, destination))
                continue;

            Path wanted = destination / filename;
            Path unique = makeUnique(wanted);
            for (uint32_t attempt = 1; !plannedDestinations.insert(unique).second; attempt++)
            {
                Path retry = destination / (wanted.stem().string() + " (" + std::to_string(attempt) + ")" + wanted.extension().string());
                unique = makeUnique(retry);
            }
            operations.push_back({ source, unique, directory });
        }
        return operations;
    }
} // namespace Crowny

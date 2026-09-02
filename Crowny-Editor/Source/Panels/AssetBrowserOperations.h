#pragma once

#include "Crowny/Common/StdHeaders.h"

#include <functional>
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

    /** One external file or folder that should be copied into the library. */
    struct AssetBrowserImportOperation
    {
        Path Source;
        Path Destination;
        bool IsDirectory = false;

        bool operator==(const AssetBrowserImportOperation&) const = default;
    };

    /**
     * Plans copies for paths dropped from the OS onto a library folder. Sources that already live in the destination
     * folder, would be copied into themselves, or repeat an earlier source are skipped. `isDirectory` reports whether a
     * source is a folder and `makeUnique` maps a wanted destination to one that does not exist on disk yet.
     */
    Vector<AssetBrowserImportOperation> PlanAssetBrowserImports(const Vector<Path>& sources, const Path& destinationFolder,
                                                                const std::function<bool(const Path&)>& isDirectory,
                                                                const std::function<Path(const Path&)>& makeUnique);

    /** Order-sensitive digest of a folder listing used to detect library changes made outside the panel. */
    struct AssetBrowserFolderFingerprint
    {
        uint64_t Hash = 14695981039346656037ull;
        size_t Count = 0;

        bool operator==(const AssetBrowserFolderFingerprint&) const = default;

        constexpr void Add(uint64_t value)
        {
            // FNV-1a over the eight value bytes, then a final avalanche so neighbouring values differ in every bit.
            for (uint32_t byte = 0; byte < 8; byte++)
            {
                Hash ^= (value >> (byte * 8)) & 0xFFull;
                Hash *= 1099511628211ull;
            }
            Hash ^= Hash >> 29;
            Hash *= 0xBF58476D1CE4E5B9ull;
            Hash ^= Hash >> 32;
        }

        constexpr void AddEntry(uint64_t identity, int64_t modifiedTime, uint64_t revision, uint64_t byteSize, bool isFile)
        {
            Add(identity);
            Add(static_cast<uint64_t>(modifiedTime));
            Add(revision);
            Add(byteSize ^ (isFile ? 0x8000000000000000ull : 0ull));
            Count++;
        }
    };

    constexpr bool IsAssetBrowserPointInside(float x, float y, float minX, float minY, float maxX, float maxY)
    {
        return maxX > minX && maxY > minY && x >= minX && y >= minY && x < maxX && y < maxY;
    }
} // namespace Crowny
